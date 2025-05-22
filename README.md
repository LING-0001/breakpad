# Breakpad

完整编译产物，直接将src复制到对应的文件位置

CMakeLists.txt
cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

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

file(GLOB BREAKPAD_ASM_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/src/common/linux/breakpad_getcontext.S)
set_source_files_properties(${BREAKPAD_ASM_SOURCE} PROPERTIES LANGUAGE C)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
        breakpad

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        ${BREAKPAD_SOURCES_COMMON} ${BREAKPAD_ASM_SOURCE} )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
        log-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
        breakpad

        # Links the target library to the log library
        # included in the NDK.
        ${log-lib} )
