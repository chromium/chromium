# Copyright 2018 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.!

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
file(STRINGS "VERSION.txt" SPM_VERSION)
message(STATUS "VERSION: ${SPM_VERSION}")

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

project(sentencepiece VERSION ${SPM_VERSION} LANGUAGES C CXX)

option(SPM_ENABLE_NFKC_COMPILE "Enables NFKC compile" OFF)
option(SPM_ENABLE_SHARED "Builds shared libaries in addition to static libraries." ON)
option(SPM_BUILD_TEST "Builds test binaries." OFF)
option(SPM_ENABLE_TCMALLOC "Enable TCMalloc if available." ON)
option(SPM_TCMALLOC_STATIC "Link static library of TCMALLOC." OFF)
option(SPM_ENABLE_MSVC_MT_BUILD, "Use /MT flag in MSVC build" OFF)
option(SPM_CROSS_SYSTEM_PROCESSOR, "Override system processor" "")
option(SPM_DISABLE_EMBEDDED_DATA, "Disable to embed pre-compiled data." OFF)
option(SPM_NLCODEC_BPE
       "Build the NLCodec fast BPE trainer from contrib/nlcodec (adds --nlcodec_bpe flag)."
       OFF)

set(SPM_PROTOBUF_PROVIDER "internal" CACHE STRING "Provider of protobuf library")
set_property(CACHE SPM_PROTOBUF_PROVIDER PROPERTY STRINGS "internal" "package")
set(SPM_ABSL_PROVIDER "internal" CACHE STRING "Provider of absl library")
set_property(CACHE SPM_ABSL_PROVIDER PROPERTY STRINGS "internal" "module" "package")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()

add_compile_definitions($<$<CONFIG:Release>:NDEBUG>)

if (SPM_ENABLE_SHARED)
 set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif() 

# Includes processor name to avoid conflicts.
set(CPACK_PACKAGE_FILE_NAME
    "sentencepiece-${SPM_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

if (SPM_CROSS_SYSTEM_PROCESSOR)
 set(CMAKE_SYSTEM_PROCESSOR ${SPM_CROSS_SYSTEM_PROCESSOR})
endif()

# Disable shared build on windows
if(WIN32)
  set(SPM_ENABLE_SHARED OFF)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if((CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND
     CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 10.0) OR
   (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
     CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 8.0))
  string(APPEND CMAKE_CXX_FLAGS " -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/=''")
endif()

if (NOT DEFINED CMAKE_INSTALL_BINDIR)
  set(CMAKE_INSTALL_BINDIR bin)
endif()

if (NOT DEFINED CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR lib)
endif()

if (NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)
  set(CMAKE_INSTALL_INCLUDEDIR include)
endif()

if (UNIX)
  include(GNUInstallDirs)
  set(prefix ${CMAKE_INSTALL_PREFIX})
  set(exec_prefix "\${prefix}")
  set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
  set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
else()
  set(prefix ${CMAKE_INSTALL_PREFIX})
  set(exec_prefix "\${prefix}")
  set(libdir "\${exec_prefix}/lib")
  set(includedir "\${prefix}/include")
endif()
set(GNUCXX_STD_SUPPORT_VERSION "4.3")

if(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
add_definitions(-D_FREEBSD)
endif()

if (SPM_USE_BUILTIN_PROTOBUF)
  set(libprotobuf_lite "")
else()
  set(libprotobuf_lite "protobuf-lite")
endif()

if (MSVC)
  add_definitions("/wd4267 /wd4244 /wd4305 /Zc:strictStrings /utf-8")
  if (SPM_ENABLE_MSVC_MT_BUILD)
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_DEBUG          ${CMAKE_CXX_FLAGS_DEBUG})
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_MINSIZEREL     ${CMAKE_CXX_FLAGS_MINSIZEREL})
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELEASE        ${CMAKE_CXX_FLAGS_RELEASE})
    string(REPLACE "/MD" "/MT" CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS_RELWITHDEBINFO})
  endif()
endif()

if (APPLE)
  set(CMAKE_MACOSX_RPATH ON)
  set(CMAKE_SKIP_BUILD_RPATH FALSE)
  set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  list(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
  if ("${isSystemDir}" STREQUAL "-1")
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
  endif()
endif()

# Add -latomic on riscv64
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "riscv64")
  string(APPEND CMAKE_C_STANDARD_LIBRARIES " -latomic")
  string(APPEND CMAKE_CXX_STANDARD_LIBRARIES " -latomic")
endif()

# SPDX-License-Identifier: (MIT OR CC0-1.0)
# Copyright 2020 Jan Tojnar
# https://github.com/jtojnar/cmake-snips
#
# Modelled after Python’s os.path.join
# https://docs.python.org/3.7/library/os.path.html#os.path.join
# Windows not supported
function(join_paths joined_path first_path_segment)
    set(temp_path "${first_path_segment}")
    foreach(current_segment IN LISTS ARGN)
        if(NOT ("${current_segment}" STREQUAL ""))
            if(IS_ABSOLUTE "${current_segment}")
                set(temp_path "${current_segment}")
            else()
                set(temp_path "${temp_path}/${current_segment}")
            endif()
        endif()
    endforeach()
    set(${joined_path} "${temp_path}" PARENT_SCOPE)
endfunction()

join_paths(libdir_for_pc_file "\${exec_prefix}" "${CMAKE_INSTALL_LIBDIR}")
join_paths(includedir_for_pc_file "\${prefix}" "${CMAKE_INSTALL_INCLUDEDIR}")
join_paths(datadir_for_pc_file "\${prefix}" "${CMAKE_INSTALL_DATADIR}" "sentencepiece")

join_paths(datadir_for_config_file "${prefix}" "${CMAKE_INSTALL_DATADIR}" "sentencepiece")
set(INSTALL_DATADIR ${datadir_for_config_file})

configure_file("${PROJECT_SOURCE_DIR}/config.h.in" "config.h")
configure_file("${PROJECT_SOURCE_DIR}/sentencepiece.pc.in" "sentencepiece.pc" @ONLY)

if (NOT MSVC)
  # suppress warning for C++11 features.
  # add_definitions("-Wno-deprecated-declarations -Wno-deprecated-enum-enum-conversion")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/sentencepiece.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_BINARY_DIR})

if (SPM_BUILD_TEST)
  enable_testing()
endif()

if (SPM_DISABLE_EMBEDDED_DATA)
 file(GLOB EMBEDDED_DATA_FILES "${CMAKE_CURRENT_SOURCE_DIR}/data/*.bin")
 install(FILES ${EMBEDDED_DATA_FILES} DESTINATION ${INSTALL_DATADIR})
endif()

if (SPM_ABSL_PROVIDER STREQUAL "internal")
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/absl)
elseif (SPM_ABSL_PROVIDER STREQUAL "module")
  include(FetchContent)
  FetchContent_Populate(abseil-cpp
        GIT_REPOSITORY  https://github.com/abseil/abseil-cpp.git
        SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/abseil-cpp
        GIT_PROGRESS TRUE
        GIT_TAG 20250814.1)
  add_subdirectory(third_party/abseil-cpp)
  if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/absl.org)
    file(RENAME ${CMAKE_CURRENT_SOURCE_DIR}/third_party/absl ${CMAKE_CURRENT_SOURCE_DIR}/third_party/absl.org)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
      ${CMAKE_CURRENT_SOURCE_DIR}/third_party/abseil-cpp/absl
      ${CMAKE_CURRENT_SOURCE_DIR}/third_party/absl)
  endif()
elseif (SPM_ABSL_PROVIDER STREQUAL "package")
  find_package(absl REQUIRED)
  get_target_property(ABSL_INCLUDE_DIRS absl::base INTERFACE_INCLUDE_DIRECTORIES)
  if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/absl.org)
    file(RENAME ${CMAKE_CURRENT_SOURCE_DIR}/third_party/absl ${CMAKE_CURRENT_SOURCE_DIR}/third_party/absl.org)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${ABSL_INCLUDE_DIRS}/absl ${CMAKE_CURRENT_SOURCE_DIR}/third_party/absl)
  endif()
  include_directories(${ABSL_INCLUDE_DIRS})
endif()

add_subdirectory(src)
add_subdirectory(third_party)

set(CPACK_SOURCE_GENERATOR "TXZ")
set(CPACK_GENERATOR "7Z")
set(CPACK_PACKAGE_VERSION "${SPM_VERSION}")
set(CPACK_STRIP_FILES TRUE)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_CONTACT "taku@google.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Taku Kudo")
set(CPACK_SOURCE_IGNORE_FILES "/build/;/.git/;/dist/;/sdist/;~$;${CPACK_SOURCE_IGNORE_FILES}")
include(CPack)
