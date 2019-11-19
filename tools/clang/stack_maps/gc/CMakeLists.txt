cmake_minimum_required(VERSION 3.13)
enable_language(ASM)

SET(CMAKE_C_COMPILER ../../../../third_party/llvm-build/Release+Asserts/bin/clang)
SET(CMAKE_CXX_COMPILER ../../../../../third_party/llvm-build/Release+Asserts/bin/clang)
SET(CMAKE_BUILD_TYPE Debug)

add_library(GC gc_api.h gc_api.cc stack_map_parser.h stack_map_parser.cc
    GC_Shim_x86_64.S)
target_compile_options(GC PUBLIC -fno-omit-frame-pointer)
target_include_directories(GC PUBLIC "../")
