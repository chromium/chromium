# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Links mimalloc into clang and lld, and into nothing else.
#
# build.py passes this file to LLVM's CMake build as
# CMAKE_PROJECT_LLVM_INCLUDE, which makes CMake include it when it processes
# LLVM's top-level project() call, and sets CHROMIUM_MIMALLOC_LIB to the path
# of libmimalloc.a.
#
# Only clang and lld get mimalloc, because an executable that brings its own
# malloc() crashes at startup if a sanitizer runtime is LD_PRELOADed into it:
# While the runtime sets up its interceptors, the dynamic loader calls
# malloc(), mimalloc initializes itself and calls clock_gettime(), and the
# runtime's interceptor for that calls the real function through a pointer
# that is not set up yet. That matters for llvm-symbolizer, which a preloaded
# asan runtime starts (with LD_PRELOAD still set) to symbolize its reports.

function(chromium_link_mimalloc)
  foreach(target clang lld)
    if(TARGET ${target})
      # --whole-archive makes sure that all of mimalloc's definitions get
      # linked in, so that they are used instead of libc's and libstdc++'s.
      # mimalloc uses pthread functions, which are in libpthread instead of
      # in libc in the sysroot's glibc (2.31). -lpthread has to come after
      # the archive, so that it's also kept with --as-needed.
      target_link_options(${target} PRIVATE
        "LINKER:--push-state,--whole-archive,${CHROMIUM_MIMALLOC_LIB},--no-whole-archive,-lpthread,--pop-state")
    endif()
  endforeach()
endfunction()

# The targets don't exist yet when this file is included. Run the function
# when CMake is done with LLVM's top-level CMakeLists.txt.
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL chromium_link_mimalloc)
