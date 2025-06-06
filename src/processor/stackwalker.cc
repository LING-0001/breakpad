// Copyright 2010 Google LLC
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google LLC nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// stackwalker.cc: Generic stackwalker.
//
// See stackwalker.h for documentation.
//
// Author: Mark Mentovai

#ifdef HAVE_CONFIG_H
#include <config.h>  // Must come first
#endif

#include "google_breakpad/processor/stackwalker.h"

#include <assert.h>

#include <memory>

#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/code_module.h"
#include "google_breakpad/processor/code_modules.h"
#include "google_breakpad/processor/dump_context.h"
#include "google_breakpad/processor/stack_frame.h"
#include "google_breakpad/processor/stack_frame_symbolizer.h"
#include "google_breakpad/processor/system_info.h"
#include "processor/linked_ptr.h"
#include "processor/logging.h"
#include "processor/stackwalker_ppc.h"
#include "processor/stackwalker_ppc64.h"
#include "processor/stackwalker_sparc.h"
#include "processor/stackwalker_x86.h"
#include "processor/stackwalker_amd64.h"
#include "processor/stackwalker_arm.h"
#include "processor/stackwalker_arm64.h"
#include "processor/stackwalker_mips.h"
#include "processor/stackwalker_riscv.h"
#include "processor/stackwalker_riscv64.h"

namespace google_breakpad {

const int Stackwalker::kRASearchWords = 40;

// This default is just a sanity check: a large enough value
// that allow capturing unbounded recursion traces, yet provide a
// guardrail against stack walking bugs. The stack walking invariants
// guarantee that the unwinding process is strictly monotonic and
// practically bounded by the size of the stack memory range.
uint32_t Stackwalker::max_frames_ = 1 << 20;  // 1M
bool Stackwalker::max_frames_set_ = false;

uint32_t Stackwalker::max_frames_scanned_ = 1 << 14;  // 16k

Stackwalker::Stackwalker(const SystemInfo* system_info,
                         MemoryRegion* memory,
                         const CodeModules* modules,
                         StackFrameSymbolizer* frame_symbolizer)
    : system_info_(system_info),
      memory_(memory),
      modules_(modules),
      unloaded_modules_(nullptr),
      frame_symbolizer_(frame_symbolizer) {
  assert(frame_symbolizer_);
}

void InsertSpecialAttentionModule(
    StackFrameSymbolizer::SymbolizerResult symbolizer_result,
    const CodeModule* module,
    vector<const CodeModule*>* modules) {
  if (!module) {
    return;
  }
  assert(symbolizer_result == StackFrameSymbolizer::kError ||
         symbolizer_result == StackFrameSymbolizer::kWarningCorruptSymbols);
  bool found = false;
  vector<const CodeModule*>::iterator iter;
  for (iter = modules->begin(); iter != modules->end(); ++iter) {
    if (*iter == module) {
      found = true;
      break;
    }
  }
  if (!found) {
    BPLOG(INFO) << ((symbolizer_result == StackFrameSymbolizer::kError) ?
                       "Couldn't load symbols for: " :
                       "Detected corrupt symbols for: ")
                << module->debug_file() << "|" << module->debug_identifier();
    modules->push_back(module);
  }
}

bool Stackwalker::Walk(
    CallStack* stack,
    vector<const CodeModule*>* modules_without_symbols,
    vector<const CodeModule*>* modules_with_corrupt_symbols) {
  BPLOG_IF(ERROR, !stack) << "Stackwalker::Walk requires |stack|";
  assert(stack);
  stack->Clear();

  BPLOG_IF(ERROR, !modules_without_symbols) << "Stackwalker::Walk requires "
                                            << "|modules_without_symbols|";
  BPLOG_IF(ERROR, !modules_without_symbols) << "Stackwalker::Walk requires "
                                            << "|modules_with_corrupt_symbols|";
  assert(modules_without_symbols);
  assert(modules_with_corrupt_symbols);

  // Begin with the context frame, and keep getting callers until there are
  // no more.

  // Keep track of the number of scanned or otherwise dubious frames seen
  // so far, as the caller may have set a limit.
  uint32_t scanned_frames = 0;

  // Take ownership of the pointer returned by GetContextFrame.
  std::unique_ptr<StackFrame> frame(GetContextFrame());

  while (frame.get()) {
    // frame already contains a good frame with properly set instruction and
    // frame_pointer fields.  The frame structure comes from either the
    // context frame (above) or a caller frame (below).

    std::deque<std::unique_ptr<StackFrame>> inlined_frames;
    // Resolve the module information, if a module map was provided.
    StackFrameSymbolizer::SymbolizerResult symbolizer_result =
        frame_symbolizer_->FillSourceLineInfo(modules_, unloaded_modules_,
                                              system_info_,
                                              frame.get(), &inlined_frames);
    switch (symbolizer_result) {
      case StackFrameSymbolizer::kInterrupt:
        BPLOG(INFO) << "Stack walk is interrupted.";
        return false;
        break;
      case StackFrameSymbolizer::kError:
        InsertSpecialAttentionModule(symbolizer_result, frame->module,
                                     modules_without_symbols);
        break;
      case StackFrameSymbolizer::kWarningCorruptSymbols:
        InsertSpecialAttentionModule(symbolizer_result, frame->module,
                                     modules_with_corrupt_symbols);
        break;
      case StackFrameSymbolizer::kNoError:
        break;
      default:
        assert(false);
        break;
    }

    // Keep track of the number of dubious frames so far.
    switch (frame.get()->trust) {
       case StackFrame::FRAME_TRUST_NONE:
       case StackFrame::FRAME_TRUST_SCAN:
       case StackFrame::FRAME_TRUST_CFI_SCAN:
         scanned_frames++;
         break;
      default:
        break;
    }
    // Add all nested inlined frames belonging to this frame from the innermost
    // frame to the outermost frame.
    while (!inlined_frames.empty()) {
      stack->frames_.push_back(inlined_frames.front().release());
      inlined_frames.pop_front();
    }
    // Add the frame to the call stack.  Relinquish the ownership claim
    // over the frame, because the stack now owns it.
    stack->frames_.push_back(frame.release());
    if (stack->frames_.size() > max_frames_) {
      // Only emit an error message in the case where the limit
      // reached is the default limit, not set by the user.
      if (!max_frames_set_)
        BPLOG(ERROR) << "The stack is over " << max_frames_ << " frames.";
      break;
    }

    // Get the next frame and take ownership.
    bool stack_scan_allowed = scanned_frames < max_frames_scanned_;
    frame.reset(GetCallerFrame(stack, stack_scan_allowed));
  }

  return true;
}

// static
Stackwalker* Stackwalker::StackwalkerForCPU(
    const SystemInfo* system_info,
    DumpContext* context,
    MemoryRegion* memory,
    const CodeModules* modules,
    const CodeModules* unloaded_modules,
    StackFrameSymbolizer* frame_symbolizer) {
  if (!context) {
    BPLOG(ERROR) << "Can't choose a stackwalker implementation without context";
    return nullptr;
  }

  Stackwalker* cpu_stackwalker = nullptr;

  uint32_t cpu = context->GetContextCPU();
  switch (cpu) {
    case MD_CONTEXT_X86:
      cpu_stackwalker = new StackwalkerX86(system_info,
                                           context->GetContextX86(),
                                           memory, modules, frame_symbolizer);
      break;

    case MD_CONTEXT_PPC:
      cpu_stackwalker = new StackwalkerPPC(system_info,
                                           context->GetContextPPC(),
                                           memory, modules, frame_symbolizer);
      break;

    case MD_CONTEXT_PPC64:
      cpu_stackwalker = new StackwalkerPPC64(system_info,
                                             context->GetContextPPC64(),
                                             memory, modules, frame_symbolizer);
      break;

    case MD_CONTEXT_AMD64:
      cpu_stackwalker = new StackwalkerAMD64(system_info,
                                             context->GetContextAMD64(),
                                             memory, modules, frame_symbolizer);
      break;

    case MD_CONTEXT_SPARC:
      cpu_stackwalker = new StackwalkerSPARC(system_info,
                                             context->GetContextSPARC(),
                                             memory, modules, frame_symbolizer);
      break;

    case MD_CONTEXT_MIPS:
    case MD_CONTEXT_MIPS64:
      cpu_stackwalker = new StackwalkerMIPS(system_info,
                                            context->GetContextMIPS(),
                                            memory, modules, frame_symbolizer);
      break;

    case MD_CONTEXT_ARM:
    {
      int fp_register = -1;
      if (system_info->os_short == "ios")
        fp_register = MD_CONTEXT_ARM_REG_IOS_FP;
      cpu_stackwalker = new StackwalkerARM(system_info,
                                           context->GetContextARM(),
                                           fp_register, memory, modules,
                                           frame_symbolizer);
      break;
    }

    case MD_CONTEXT_ARM64:
      cpu_stackwalker = new StackwalkerARM64(system_info,
                                             context->GetContextARM64(),
                                             memory, modules,
                                             frame_symbolizer);
      break;

    case MD_CONTEXT_RISCV:
      cpu_stackwalker = new StackwalkerRISCV(system_info,
                                             context->GetContextRISCV(),
                                             memory, modules,
                                             frame_symbolizer);
      break;

    case MD_CONTEXT_RISCV64:
      cpu_stackwalker = new StackwalkerRISCV64(system_info,
                                               context->GetContextRISCV64(),
                                               memory, modules,
                                               frame_symbolizer);
      break;
  }

  BPLOG_IF(ERROR, !cpu_stackwalker) << "Unknown CPU type " << HexString(cpu) <<
                                       ", can't choose a stackwalker "
                                       "implementation";
  if (cpu_stackwalker) {
    cpu_stackwalker->unloaded_modules_ = unloaded_modules;
  }
  return cpu_stackwalker;
}

// CONSIDER: check stack alignment?
bool Stackwalker::TerminateWalk(uint64_t caller_ip,
                                uint64_t caller_sp,
                                uint64_t callee_sp,
                                bool first_unwind) const {
  // Treat an instruction address less than 4k as end-of-stack.
  // (using InstructionAddressSeemsValid() here is very tempting,
  // but we need to handle JITted code)
  if (caller_ip < (1 << 12)) {
    return true;
  }

  // NOTE: The stack address range is implicitly checked
  //   when the stack memory is accessed.

  // The stack pointer should monotonically increase. For first unwind
  // we allow caller_sp == callee_sp to account for architectures where
  // the return address is stored in a register (so it's possible to have
  // leaf functions which don't move the stack pointer)
  if (first_unwind ? (caller_sp < callee_sp) : (caller_sp <= callee_sp)) {
    return true;
  }

  return false;
}

bool Stackwalker::InstructionAddressSeemsValid(uint64_t address) const {
  StackFrame frame;
  frame.instruction = address;
  StackFrameSymbolizer::SymbolizerResult symbolizer_result =
      frame_symbolizer_->FillSourceLineInfo(modules_, unloaded_modules_,
                                            system_info_, &frame, nullptr);

  if (!frame.module) {
    // not inside any loaded module
    return false;
  }

  if (!frame_symbolizer_->HasImplementation()) {
    // No valid implementation to symbolize stack frame, but the address is
    // within a known module.
    return true;
  }

  if (symbolizer_result != StackFrameSymbolizer::kNoError &&
      symbolizer_result != StackFrameSymbolizer::kWarningCorruptSymbols) {
    // Some error occurred during symbolization, but the address is within a
    // known module
    return true;
  }

  return !frame.function_name.empty();
}

}  // namespace google_breakpad
