
> **Breakpad**是一个跨平台的**开源崩溃收集与分析工具**，用于在程序崩溃时生成**minidump 崩溃转储文件**，便于在本地或服务端还原出可读的 **堆栈信息（stack trace）** 。
>
> 本文介绍了在 Windows 环境下编译 Breakpad 并集成至 Android 项目的完整流程。
>
> 如在操作中遇到问题，欢迎阅读本文或在评论区交流。也可以直接使用已编译好的版本，快速集成至本地项目，地址如下：<https://github.com/LING-0001/breakpad>

### 一、准备工作

1.安装MSYS2：<https://www.msys2.org>

2.使用MSYS2安装make工具

```shell
pacman -S base-devel
```

3.使用MSYS2安装mingw编译器

```shell
pacman -S git make cmake python3 mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
```

> 说明：主要是通过在 Windows 上模拟 Linux 编译环境来完成构建，也可以选择其他方式自行搭建 C++ 编译环境（如 MinGW-64 等）。

### 二、下载并编译breakpad

1.下载对应的github项目，动手能力强的可以直接去breakpad官网，链接如下：<https://github.com/google/breakpad>

```shell
git clone https://chromium.googlesource.com/breakpad/breakpad
git clone https://chromium.googlesource.com/external/gyp breakpad/src/third_party/gyp
git clone https://chromium.googlesource.com/linux-syscall-support src/third_party/lss
//不下载编译会报错'third_party/lss/linux_syscall_support. h' file not found
```

2.MSYS2 MINGW64打开`breakpad`目录

3.保险起见编译前清除缓存

```shell
make clean
```

4.编译breakpad拿到minidump\_stackwalk

```shell
./configure && make
```

⚠️**这一步比较关键**，如果直接在windows的shell环境下编译可能会出错。

### 三、集成Android项目

1.将breakpad编译产物的src文件复制到合适位置

<img src="https://i-blog.csdnimg.cn/direct/febba10cc021473e8e19fb17724c1626.png" style="float: left;" alt="左对齐图片" width="50%">

2.配置CMakeLists

```cpp
cmake_minimum_required(VERSION 3.4.1)
​
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})
​
include_directories(    ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/dump_writer_common
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/linux)
file(GLOB BREAKPAD_SOURCES_COMMON
        native-lib.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/crash_generation/crash_generation_client.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/dump_writer_common/thread_info.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/dump_writer_common/ucontext_reader.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/handler/exception_handler.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/handler/minidump_descriptor.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/log/log.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/microdump_writer/microdump_writer.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/minidump_writer/linux_dumper.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/minidump_writer/linux_ptrace_dumper.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/linux/minidump_writer/minidump_writer.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/minidump_file_writer.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/convert_UTF.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/md5.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/string_conversion.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/linux/elfutils.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/linux/file_id.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/linux/guid_creator.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/linux/linux_libc_support.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/linux/memory_mapped_file.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/common/linux/safe_readlink.cc
)
​
file(GLOB BREAKPAD_ASM_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/common/linux/breakpad_getcontext.S)
set_source_files_properties(${BREAKPAD_ASM_SOURCE} PROPERTIES LANGUAGE C)
​
# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
​
add_library( # Sets the name of the library.
        breakpad
​
        # Sets the library as a shared library.
        SHARED
​
        # Provides a relative path to your source file(s).
        ${BREAKPAD_SOURCES_COMMON} ${BREAKPAD_ASM_SOURCE} )
​
# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.
​
find_library( # Sets the name of the path variable.
        log-lib
​
        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log )
​
# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.
​
target_link_libraries( # Specifies the target library.
        breakpad
​
        # Links the target library to the log library
        # included in the NDK.
        ${log-lib} )
```

3.编写jni代码

```cpp
#include <jni.h>
#include <string>
#include "android/log.h"
#include "src/client/linux/handler/minidump_descriptor.h"
#include "src/client/linux/handler/exception_handler.h"
​
const char *TAG = "Breakpad";
#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO,  TAG, fmt, ##args)
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, TAG, fmt, ##args)
#define LOGE(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, TAG, fmt, ##args)
​
// 崩溃回调
bool DumpCallback(const google_breakpad::MinidumpDescriptor &descriptor,
                  void *context,
                  bool succeeded) {
​
    LOGE("Dump path: %s\n", descriptor.path());
    return succeeded;
}
​
// 模拟Crash
void Crash() {
    volatile int *a = reinterpret_cast<volatile int *>(NULL);
    *a = 1;
}
​
extern "C"
JNIEXPORT void JNICALL
Java_com_example_jni_JniActivity_initBreakpad(JNIEnv *env, jobject,
                                              jstring path_) {
    const char *path = env->GetStringUTFChars(path_, 0);
​
    google_breakpad::MinidumpDescriptor descriptor(path);
    static google_breakpad::ExceptionHandler eh(descriptor, NULL, DumpCallback, NULL, true, -1);
​
    env->ReleaseStringUTFChars(path_, path);
}
​
extern "C"
JNIEXPORT void JNICALL
Java_com_example_jni_JniActivity_crash(JNIEnv *env, jobject) {
    Crash();
}
​
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    return JNI_VERSION_1_6;
}
```

4.安卓代码

```kotlin
class JniActivity : BaseActivity<ActivityJniBinding>() {

    companion object {
        init {
            System.loadLibrary("breakpad")
        }
    }

    private external fun initBreakpad(breakpadPath: String)

    external fun crash()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        initBreakpad(filesDir.absolutePath)
        initView()
    }

    private fun initView() {
        binding?.tvJni?.setOnClickListener {
            crash()
            startNativeThread()
        }
    }
}
```

5.运行，然后编译报错，因为windows下需要保留 ELF 分支，移除 PE 分支

找到minidump\_writer.cc

```cpp
//    RSDS_DEBUG_FORMAT rsds;
//    PEFileFormat file_format = PEFile::TryGetDebugInfo(file_path, &rsds);
​
if (true) {
    保留
} else {
    //注释
}
​
注释选择分支逻辑，直接走elf分支
```

6.点击按钮闪退，找到储存的dump文件

7.执行shell脚本

```shell
自己的路径./minidump_stackwalk.exe 生成dump的路径.dmp > textLog.txt
```

生成信息如下，会告诉你一些机型信息，以及闪退的对应符号表

```text
Operating system: Android
                  0.0.0 Linux 4.14.116 #1 SMP PREEMPT Wed Jan 24 00:41:44 CST 2024 aarch64
CPU: arm64
     8 CPUs

GPU: UNKNOWN

Crash reason:  SIGSEGV /SEGV_MAPERR
Crash address: 0x0
Process uptime: not available

Thread 0 (crashed)
 0  libbreakpad.so + 0x5014c
     x0 = 0x00000073eccf3980    x1 = 0x0000007fed5cbbc4
     x2 = 0x0000000000000010    x3 = 0x000000000000001c
     x4 = 0x0000000000000001    x5 = 0x0000007fed5cbb80
     x6 = 0x0000007fed5cbbe8    x7 = 0x0000007fed5cbc00
     x8 = 0x0000000000000001    x9 = 0x0000000000000000
    x10 = 0x0000000000430000   x11 = 0x00000073ecbd3000
    x12 = 0x0000007470026560   x13 = 0x3e795d9ee7e38c19
    x14 = 0x0000000000000006   x15 = 0xffffffffffffffff
    x16 = 0x00000073e016f340   x17 = 0x00000073e011313c
    x18 = 0x000000747315c000   x19 = 0x00000073ecc10800
    x20 = 0x0000000000000000   x21 = 0x00000073ecc10800
    x22 = 0x0000007fed5cbe30   x23 = 0x000000746d633246
    x24 = 0x0000000000000004   x25 = 0x0000007472705020
    x26 = 0x00000073ecc108b0   x27 = 0x0000000000000001
    x28 = 0x0000007fed5cbbc0    fp = 0x0000007fed5cbb90
```

8.找到Android studio ndk自带的aarch64-linux-android-addr2line，以及编译成功的so文件，将报错符号解析成可读懂代码

```shell
自己的路径./aarch64-linux-android-addr2line.exe -f -C -e 生成的so文件的路径.so 0x5014c(对应报错的内容)
```

生成信息如下：

<img src="https://i-blog.csdnimg.cn/direct/5124122d42e341fe86776984019abf08.png" style="float: left;" alt="左对齐图片" width="70%">

完事，找到了报错的地方，Crash函数以及29行

### 五、更多的意义

1.可编写shell脚本，实现闪退信息的自动化解析与归档。

2.可hook关键节点，实现 dump文件的上传、汇报或接入监控平台。

3.学习
