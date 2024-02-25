// Copyright 2017 The Crashpad Authors
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

#include "util/process/process_memory_linux.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <limits>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "util/file/filesystem.h"
#include "util/linux/ptrace_connection.h"

namespace crashpad {

ProcessMemoryLinux::ProcessMemoryLinux(PtraceConnection* connection)
    : ProcessMemory(), mem_fd_(), ignore_top_byte_(false) {
#if defined(ARCH_CPU_ARM_FAMILY)
  if (connection->Is64Bit()) {
    ignore_top_byte_ = true;
  }
#endif  // ARCH_CPU_ARM_FAMILY

  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/mem", connection->GetProcessID());
  mem_fd_.reset(HANDLE_EINTR(open(path, O_RDONLY | O_NOCTTY | O_CLOEXEC)));
  if (mem_fd_.is_valid()) {
    read_up_to_ = [this](VMAddress address, size_t size, void* buffer) {
      ssize_t bytes_read =
          HANDLE_EINTR(pread64(mem_fd_.get(), buffer, size, address));
      if (bytes_read < 0) {
        PLOG(ERROR) << "pread64";
      }
      return bytes_read;
    };
    return;
  }

  read_up_to_ = std::bind(&PtraceConnection::ReadUpTo,
                          connection,
                          std::placeholders::_1,
                          std::placeholders::_2,
                          std::placeholders::_3);
}

ProcessMemoryLinux::~ProcessMemoryLinux() {}

VMAddress ProcessMemoryLinux::PointerToAddress(VMAddress address) const {
  return ignore_top_byte_ ? address & 0x00ffffffffffffff : address;
}

ssize_t ProcessMemoryLinux::ReadUpTo(VMAddress address,
                                     size_t size,
                                     void* buffer) const {
  DCHECK_LE(size, size_t{std::numeric_limits<ssize_t>::max()});
  return read_up_to_(PointerToAddress(address), size, buffer);
}

}  // namespace crashpad
