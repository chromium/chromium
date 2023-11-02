// Copyright 2015 The Crashpad Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "util/stdlib/aligned_allocator.h"

#include <algorithm>

#include "build/build_config.h"

#if BUILDFLAG(IS_POSIX) || defined(_LIBCPP_STD_VER)
#include <stdlib.h>
#elif BUILDFLAG(IS_WIN)
#include <malloc.h>
#include <xutility>
#endif  // BUILDFLAG(IS_POSIX)

namespace {

// Throws std::bad_alloc() by calling an internal function provided by the C++
// library to do so. This works even if C++ exceptions are disabled, causing
// program termination if uncaught.
void ThrowBadAlloc() {
#if BUILDFLAG(IS_POSIX) || defined(_LIBCPP_STD_VER)
  // This works with both libc++ and libstdc++.
  std::__throw_bad_alloc();
#elif BUILDFLAG(IS_WIN)
  std::_Xbad_alloc();
#endif  // BUILDFLAG(IS_POSIX)
}

}  // namespace

namespace crashpad {

void* AlignedAllocate(size_t alignment, size_t size) {
#if BUILDFLAG(IS_POSIX)
  // posix_memalign() requires that alignment be at least sizeof(void*), so the
  // power-of-2 check needs to happen before potentially changing the alignment.
  if (alignment == 0 || alignment & (alignment - 1)) {
    ThrowBadAlloc();
  }

  void* pointer;
  if (posix_memalign(&pointer, std::max(alignment, sizeof(void*)), size) != 0) {
    ThrowBadAlloc();
  }
#elif BUILDFLAG(IS_WIN)
  void* pointer = _aligned_malloc(size, alignment);
  if (pointer == nullptr) {
    ThrowBadAlloc();
  }
#endif  // BUILDFLAG(IS_POSIX)

  return pointer;
}

void AlignedFree(void* pointer) {
#if BUILDFLAG(IS_POSIX)
  free(pointer);
#elif BUILDFLAG(IS_WIN)
  _aligned_free(pointer);
#endif  // BUILDFLAG(IS_POSIX)
}

}  // namespace crashpad
