// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "util/posix/scoped_mmap.h"

#include <unistd.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/numerics/safe_math.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"

#if defined(OS_LINUX)
#include "third_party/lss/lss.h"
#endif

namespace {

#if defined(OS_LINUX)
void* CallMmap(void* addr,
               size_t len,
               int prot,
               int flags,
               int fd,
               off_t offset) {
  return sys_mmap(addr, len, prot, flags, fd, offset);
}

int CallMunmap(void* addr, size_t len) {
  return sys_munmap(addr, len);
}

int CallMprotect(void* addr, size_t len, int prot) {
  return sys_mprotect(addr, len, prot);
}
#else
void* CallMmap(void* addr,
               size_t len,
               int prot,
               int flags,
               int fd,
               off_t offset) {
  return mmap(addr, len, prot, flags, fd, offset);
}

int CallMunmap(void* addr, size_t len) {
  return munmap(addr, len);
}

int CallMprotect(void* addr, size_t len, int prot) {
  return mprotect(addr, len, prot);
}
#endif

bool LoggingMunmap(uintptr_t addr, size_t len, bool can_log) {
  if (CallMunmap(reinterpret_cast<void*>(addr), len) != 0) {
    PLOG_IF(ERROR, can_log) << "munmap";
    return false;
  }

  return true;
}

size_t RoundPage(size_t size) {
  const size_t kPageMask = base::checked_cast<size_t>(base::GetPageSize()) - 1;
  return (size + kPageMask) & ~kPageMask;
}

}  // namespace

namespace crashpad {

ScopedMmap::ScopedMmap(bool can_log) : can_log_(can_log) {}

ScopedMmap::~ScopedMmap() {
  if (is_valid()) {
    LoggingMunmap(
        reinterpret_cast<uintptr_t>(addr_), RoundPage(len_), can_log_);
  }
}

bool ScopedMmap::Reset() {
  return ResetAddrLen(MAP_FAILED, 0);
}

bool ScopedMmap::ResetAddrLen(void* addr, size_t len) {
  const uintptr_t new_addr = reinterpret_cast<uintptr_t>(addr);
  const size_t new_len_round = RoundPage(len);

  if (addr == MAP_FAILED) {
    DCHECK_EQ(len, 0u);
  } else {
    DCHECK_NE(len, 0u);
    DCHECK_EQ(new_addr % base::GetPageSize(), 0u);
    DCHECK((base::CheckedNumeric<uintptr_t>(new_addr) + (new_len_round - 1))
               .IsValid());
  }

  bool result = true;

  if (is_valid()) {
    const uintptr_t old_addr = reinterpret_cast<uintptr_t>(addr_);
    const size_t old_len_round = RoundPage(len_);
    if (old_addr < new_addr) {
      result &= LoggingMunmap(
          old_addr, std::min(old_len_round, new_addr - old_addr), can_log_);
    }
    if (old_addr + old_len_round > new_addr + new_len_round) {
      uintptr_t unmap_start = std::max(old_addr, new_addr + new_len_round);
      result &= LoggingMunmap(
          unmap_start, old_addr + old_len_round - unmap_start, can_log_);
    }
  }

  addr_ = addr;
  len_ = len;

  return result;
}

bool ScopedMmap::ResetMmap(void* addr,
                           size_t len,
                           int prot,
                           int flags,
                           int fd,
                           off_t offset) {
  // Reset() first, so that a new anonymous mapping can use the address space
  // occupied by the old mapping if appropriate. The new mapping will be
  // attempted even if there was something wrong with the old mapping, so don’t
  // consider the return value from Reset().
  Reset();

  void* new_addr = CallMmap(addr, len, prot, flags, fd, offset);
  if (new_addr == MAP_FAILED) {
    PLOG_IF(ERROR, can_log_) << "mmap";
    return false;
  }

  // The new mapping is effective even if there was something wrong with the old
  // mapping, so don’t consider the return value from ResetAddrLen().
  ResetAddrLen(new_addr, len);
  return true;
}

bool ScopedMmap::Mprotect(int prot) {
  if (CallMprotect(addr_, RoundPage(len_), prot) < 0) {
    PLOG_IF(ERROR, can_log_) << "mprotect";
    return false;
  }

  return true;
}

void* ScopedMmap::release() {
  void* retval = addr_;
  addr_ = MAP_FAILED;
  len_ = 0;
  return retval;
}

}  // namespace crashpad
