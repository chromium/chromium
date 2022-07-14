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
#include <mach/mach.h>
#include <unistd.h>

#include <algorithm>
#include <ostream>

#include "base/check.h"
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

IOSIntermediateDumpWriter::~IOSIntermediateDumpWriter() {
  DCHECK_EQ(fd_, -1) << "Call Close() before this object is destroyed.";
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

  return true;
}

bool IOSIntermediateDumpWriter::Close() {
  if (fd_ < 0) {
    return true;
  }
  int fd = fd_;
  fd_ = -1;
  return RawLoggingCloseFile(fd);
}

bool IOSIntermediateDumpWriter::AddPropertyCString(IntermediateDumpKey key,
                                                   size_t max_length,
                                                   const char* value) {
  constexpr size_t kMaxStringBytes = 1024;
  if (max_length > kMaxStringBytes) {
    CRASHPAD_RAW_LOG("AddPropertyCString max_length too large");
    return false;
  }

  char buffer[kMaxStringBytes];
  size_t string_length;
  if (ReadCStringInternal(value, buffer, max_length, &string_length)) {
    return Property(key, buffer, string_length);
  }
  return false;
}

bool IOSIntermediateDumpWriter::ReadCStringInternal(const char* value,
                                                    char* buffer,
                                                    size_t max_length,
                                                    size_t* string_length) {
  size_t length = 0;
  while (length < max_length) {
    vm_address_t data_address = reinterpret_cast<vm_address_t>(value + length);
    // Calculate bytes to read past `data_address`, either the number of bytes
    // to the end of the page, or the remaining bytes in `buffer`, whichever is
    // smaller.
    size_t data_to_end_of_page =
        getpagesize() - (data_address - trunc_page(data_address));
    size_t remaining_bytes_in_buffer = max_length - length;
    size_t bytes_to_read =
        std::min(data_to_end_of_page, remaining_bytes_in_buffer);

    char* buffer_start = buffer + length;
    size_t bytes_read = 0;
    kern_return_t kr =
        vm_read_overwrite(mach_task_self(),
                          data_address,
                          bytes_to_read,
                          reinterpret_cast<vm_address_t>(buffer_start),
                          &bytes_read);
    if (kr != KERN_SUCCESS || bytes_read <= 0) {
      CRASHPAD_RAW_LOG("ReadCStringInternal vm_read_overwrite failed");
      return false;
    }

    char* nul = static_cast<char*>(memchr(buffer_start, '\0', bytes_read));
    if (nul != nullptr) {
      length += nul - buffer_start;
      *string_length = length;
      return true;
    }
    length += bytes_read;
  }
  CRASHPAD_RAW_LOG("unterminated string");
  return false;
}

bool IOSIntermediateDumpWriter::AddPropertyInternal(IntermediateDumpKey key,
                                                    const char* value,
                                                    size_t value_length) {
  ScopedVMRead<char> vmread;
  if (!vmread.Read(value, value_length))
    return false;
  return Property(key, vmread.get(), value_length);
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

bool IOSIntermediateDumpWriter::Property(IntermediateDumpKey key,
                                         const void* value,
                                         size_t value_length) {
  const CommandType command_type = CommandType::kProperty;
  return RawLoggingWriteFile(fd_, &command_type, sizeof(command_type)) &&
         RawLoggingWriteFile(fd_, &key, sizeof(key)) &&
         RawLoggingWriteFile(fd_, &value_length, sizeof(size_t)) &&
         RawLoggingWriteFile(fd_, value, value_length);
}

}  // namespace internal
}  // namespace crashpad
