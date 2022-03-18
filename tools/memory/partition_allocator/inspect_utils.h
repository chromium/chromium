// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_MEMORY_PARTITION_ALLOCATOR_INSPECT_UTILS_H_
#define TOOLS_MEMORY_PARTITION_ALLOCATOR_INSPECT_UTILS_H_

// This file includes utilities used in partition alloc tools, also
// found in this directory.

#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/file.h"
#include "base/posix/eintr_wrapper.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace partition_alloc::internal::tools {

// SIGSTOPs a process.
class ScopedSigStopper {
 public:
  explicit ScopedSigStopper(pid_t pid) : pid_(pid) { kill(pid_, SIGSTOP); }
  ~ScopedSigStopper() { kill(pid_, SIGCONT); }

 private:
  const pid_t pid_;
};

base::ScopedFD OpenProcMem(pid_t pid);

bool ReadMemory(int fd, unsigned long address, size_t size, char* buffer);

uintptr_t IndexThreadCacheNeedleArray(pid_t pid, int mem_fd, size_t index);

// Allows to access an object copied from remote memory "as if" it were
// local. Of course, dereferencing any pointer from within it will at best
// fault.
template <typename T>
class RawBuffer {
 public:
  RawBuffer() = default;
  const T* get() const { return reinterpret_cast<const T*>(buffer_); }
  char* get_buffer() { return buffer_; }

  static absl::optional<RawBuffer<T>> ReadFromMemFd(int mem_fd,
                                                    uintptr_t address) {
    RawBuffer<T> buf;
    bool ok = ReadMemory(mem_fd, reinterpret_cast<unsigned long>(address),
                         sizeof(T), buf.get_buffer());
    if (!ok)
      return absl::nullopt;

    return {buf};
  }

 private:
  alignas(T) char buffer_[sizeof(T)];
};

}  // namespace partition_alloc::internal::tools

#endif  // TOOLS_MEMORY_PARTITION_ALLOCATOR_INSPECT_UTILS_H_
