cmake_minimum_required(VERSION 3.1)

find_package(LLVM REQUIRED CONFIG)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_LIBRARY_DIRS})

add_subdirectory(RegisterGcFunctions)
add_subdirectory(IdentifySafepoints)
