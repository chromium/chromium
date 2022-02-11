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

}  // namespace partition_alloc::internal::tools

#endif  // TOOLS_MEMORY_PARTITION_ALLOCATOR_INSPECT_UTILS_H_
