// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include "util/ios/ios_intermediate_dump_writer.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "util/ios/raw_logging.h"
#include "util/ios/scoped_vm_read.h"

namespace crashpad {
namespace internal {

// Similar to LoggingWriteFile but with CRASHPAD_RAW_LOG.
bool RawLoggingWriteFile(int fd, const void* buffer, size_t size) {
  uintptr_t buffer_int = reinterpret_cast<uintptr_t>(buffer);
  while (size > 0) {
    ssize_t bytes_written = HANDLE_EINTR(
        write(fd, reinterpret_cast<const char*>(buffer_int), size));
    if (bytes_written < 0 || bytes_written == 0) {
      CRASHPAD_RAW_LOG_ERROR(bytes_written, "RawLoggingWriteFile");
      return false;
    }
    buffer_int += bytes_written;
    size -= bytes_written;
  }
  return true;
}

// Similar to LoggingCloseFile but with CRASHPAD_RAW_LOG.
bool RawLoggingCloseFile(int fd) {
  int rv = IGNORE_EINTR(close(fd));
  if (rv != 0) {
    CRASHPAD_RAW_LOG_ERROR(rv, "RawLoggingCloseFile");
  }
  return rv == 0;
}

// Similar to LoggingLockFile but with CRASHPAD_RAW_LOG.
bool RawLoggingLockFileExclusiveNonBlocking(int fd) {
  int rv = HANDLE_EINTR(flock(fd, LOCK_EX | LOCK_NB));
  if (rv != 0) {
    CRASHPAD_RAW_LOG_ERROR(rv, "RawLoggingLockFileExclusiveNonBlocking");
  }
  return rv == 0;
}

bool IOSIntermediateDumpWriter::Open(const base::FilePath& path) {
  // Set data protection class D (No protection). A file with this type of
  // protection can be read from or written to at any time.
  // See:
  // https://support.apple.com/guide/security/data-protection-classes-secb010e978a/web
  constexpr int PROTECTION_CLASS_D = 4;
  fd_ = HANDLE_EINTR(open_dprotected_np(path.value().c_str(),
                                        O_WRONLY | O_CREAT | O_TRUNC,
                                        PROTECTION_CLASS_D,
                                        0 /* dpflags */,
                                        0644 /* mode */));
  if (fd_ < 0) {
    CRASHPAD_RAW_LOG_ERROR(fd_, "open intermediate dump");
    CRASHPAD_RAW_LOG(path.value().c_str());
    return false;
  }

  return RawLoggingLockFileExclusiveNonBlocking(fd_);
}

bool IOSIntermediateDumpWriter::Close() {
  return RawLoggingCloseFile(fd_);
}

bool IOSIntermediateDumpWriter::ArrayMapStart() {
  const CommandType command_type = CommandType::kMapStart;
  return RawLoggingWriteFile(fd_, &command_type, sizeof(command_type));
}

bool IOSIntermediateDumpWriter::MapStart(IntermediateDumpKey key) {
  const CommandType command_type = CommandType::kMapStart;
  return RawLoggingWriteFile(fd_, &command_type, sizeof(command_type)) &&
         RawLoggingWriteFile(fd_, &key, sizeof(key));
}

bool IOSIntermediateDumpWriter::ArrayStart(IntermediateDumpKey key) {
  const CommandType command_type = CommandType::kArrayStart;
  return RawLoggingWriteFile(fd_, &command_type, sizeof(command_type)) &&
         RawLoggingWriteFile(fd_, &key, sizeof(key));
}

bool IOSIntermediateDumpWriter::MapEnd() {
  const CommandType command_type = CommandType::kMapEnd;
  return RawLoggingWriteFile(fd_, &command_type, sizeof(command_type));
}

bool IOSIntermediateDumpWriter::ArrayEnd() {
  const CommandType command_type = CommandType::kArrayEnd;
  return RawLoggingWriteFile(fd_, &command_type, sizeof(command_type));
}

bool IOSIntermediateDumpWriter::RootMapStart() {
  const CommandType command_type = CommandType::kRootMapStart;
  return RawLoggingWriteFile(fd_, &command_type, sizeof(command_type));
}

bool IOSIntermediateDumpWriter::RootMapEnd() {
  const CommandType command_type = CommandType::kRootMapEnd;
  return RawLoggingWriteFile(fd_, &command_type, sizeof(command_type));
}

bool IOSIntermediateDumpWriter::AddPropertyInternal(IntermediateDumpKey key,
                                                    const char* value,
                                                    size_t value_length) {
  ScopedVMRead<char> vmread;
  if (!vmread.Read(value, value_length))
    return false;
  const CommandType command_type = CommandType::kProperty;
  return RawLoggingWriteFile(fd_, &command_type, sizeof(command_type)) &&
         RawLoggingWriteFile(fd_, &key, sizeof(key)) &&
         RawLoggingWriteFile(fd_, &value_length, sizeof(size_t)) &&
         RawLoggingWriteFile(fd_, vmread.get(), value_length);
}

}  // namespace internal
}  // namespace crashpad
