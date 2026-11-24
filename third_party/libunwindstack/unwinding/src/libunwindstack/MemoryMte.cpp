/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/ptrace.h>
#include <sys/uio.h>

#if defined(__BIONIC__) && !defined(__ANDROID_NDK__)
#include <bionic/mte.h>
#else
#define mte_supported() false
#endif

#include "MemoryLocal.h"
#include "MemoryRemote.h"

namespace unwindstack {

long MemoryRemote::ReadTag(uint64_t addr) {
#if defined(PTRACE_PEEKMTETAGS) || defined(PT_PEEKMTETAGS)
  char tag;
  iovec iov = {&tag, 1};
  if (ptrace(PTRACE_PEEKMTETAGS, pid_, reinterpret_cast<void*>(addr), &iov) != 0 ||
      iov.iov_len != 1) {
    return -1;
  }
  return tag;
#else
  (void)addr;
  return -1;
#endif
}

long MemoryLocal::ReadTag(uint64_t addr) {
#if defined(__aarch64__)
  // Check that the memory is readable first. This is racy with the ldg but there's not much
  // we can do about it.
  char data;
  if (!mte_supported() || !Read(addr, &data, 1)) {
    return -1;
  }

  __asm__ __volatile__(".arch_extension mte; ldg %0, [%0]" : "+r"(addr) : : "memory");
  return (addr >> 56) & 0xf;
#else
  (void)addr;
  return -1;
#endif
}

}  // namespace unwindstack
