// Copyright 2014 The Crashpad Authors
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

#include "util/file/file_io.h"

#include <functional>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace crashpad {

namespace {

FileOperationResult FileIORead(FileHandle file,
                               bool can_log,
                               void* buffer,
                               size_t size) {
  FileOperationResult rv = ReadFile(file, buffer, size);
  if (rv < 0) {
    PLOG_IF(ERROR, can_log) << internal::kNativeReadFunctionName;
    return -1;
  }
  return rv;
}

class FileIOWriteAll final : public internal::WriteAllInternal {
 public:
  explicit FileIOWriteAll(FileHandle file) : WriteAllInternal(), file_(file) {}

  FileIOWriteAll(const FileIOWriteAll&) = delete;
  FileIOWriteAll& operator=(const FileIOWriteAll&) = delete;

  ~FileIOWriteAll() {}

 private:
  // WriteAllInternal:
  FileOperationResult Write(const void* buffer, size_t size) override {
    return internal::NativeWriteFile(file_, buffer, size);
  }

  FileHandle file_;
};

FileOperationResult ReadUntil(
    std::function<FileOperationResult(void*, size_t)> read_function,
    void* buffer,
    size_t size) {
  // Ensure bytes read fit within int32_t::max to make sure that they also fit
  // into FileOperationResult on all platforms.
  DCHECK_LE(size, size_t{std::numeric_limits<int32_t>::max()});
  uintptr_t buffer_int = reinterpret_cast<uintptr_t>(buffer);
  size_t total_bytes = 0;
  size_t remaining = size;
  while (remaining > 0) {
    const FileOperationResult bytes_read =
        read_function(reinterpret_cast<char*>(buffer_int), remaining);
    if (bytes_read < 0) {
      return bytes_read;
    }

    DCHECK_LE(static_cast<size_t>(bytes_read), remaining);

    if (bytes_read == 0) {
      break;
    }

    buffer_int += bytes_read;
    remaining -= bytes_read;
    total_bytes += bytes_read;
  }
  return total_bytes;
}

}  // namespace

namespace internal {

bool ReadExactly(
    std::function<FileOperationResult(bool, void*, size_t)> read_function,
    bool can_log,
    void* buffer,
    size_t size) {
  const FileOperationResult result =
      ReadUntil(std::bind_front(read_function, can_log), buffer, size);
  if (result < 0) {
    return false;
  }

  if (static_cast<size_t>(result) != size) {
    LOG_IF(ERROR, can_log) << "ReadExactly: expected " << size << ", observed "
                           << result;
    return false;
  }

  return true;
}

bool WriteAllInternal::WriteAll(const void* buffer, size_t size) {
  uintptr_t buffer_int = reinterpret_cast<uintptr_t>(buffer);

  while (size > 0) {
    FileOperationResult bytes_written =
        Write(reinterpret_cast<const char*>(buffer_int), size);
    if (bytes_written < 0) {
      return false;
    }

    DCHECK_NE(bytes_written, 0);

    buffer_int += bytes_written;
    size -= bytes_written;
  }

  return true;
}

}  // namespace internal

bool ReadFileExactly(FileHandle file, void* buffer, size_t size) {
  return internal::ReadExactly(
      std::bind_front(&FileIORead, file), false, buffer, size);
}

FileOperationResult ReadFileUntil(FileHandle file, void* buffer, size_t size) {
  return ReadUntil(std::bind_front(&FileIORead, file, false), buffer, size);
}

bool LoggingReadFileExactly(FileHandle file, void* buffer, size_t size) {
  return internal::ReadExactly(
      std::bind_front(&FileIORead, file), true, buffer, size);
}

FileOperationResult LoggingReadFileUntil(FileHandle file,
                                         void* buffer,
                                         size_t size) {
  return ReadUntil(std::bind_front(&FileIORead, file, true), buffer, size);
}

bool WriteFile(FileHandle file, const void* buffer, size_t size) {
  FileIOWriteAll write_all(file);
  return write_all.WriteAll(buffer, size);
}

bool LoggingWriteFile(FileHandle file, const void* buffer, size_t size) {
  if (!WriteFile(file, buffer, size)) {
    PLOG(ERROR) << internal::kNativeWriteFunctionName;
    return false;
  }

  return true;
}

void CheckedReadFileExactly(FileHandle file, void* buffer, size_t size) {
  CHECK(LoggingReadFileExactly(file, buffer, size));
}

void CheckedWriteFile(FileHandle file, const void* buffer, size_t size) {
  CHECK(LoggingWriteFile(file, buffer, size));
}

void CheckedReadFileAtEOF(FileHandle file) {
  char c;
  FileOperationResult rv = ReadFile(file, &c, 1);
  if (rv < 0) {
    PCHECK(rv == 0) << internal::kNativeReadFunctionName;
  } else {
    CHECK_EQ(rv, 0) << internal::kNativeReadFunctionName;
  }
}

bool LoggingReadToEOF(FileHandle file, std::string* contents) {
  char buffer[4096];
  FileOperationResult rv;
  std::string local_contents;
  while ((rv = ReadFile(file, buffer, sizeof(buffer))) > 0) {
    DCHECK_LE(static_cast<size_t>(rv), sizeof(buffer));
    local_contents.append(buffer, rv);
  }
  if (rv < 0) {
    PLOG(ERROR) << internal::kNativeReadFunctionName;
    return false;
  }
  contents->swap(local_contents);
  return true;
}

bool LoggingReadEntireFile(const base::FilePath& path, std::string* contents) {
  ScopedFileHandle handle(LoggingOpenFileForRead(path));
  if (!handle.is_valid()) {
    return false;
  }

  return LoggingReadToEOF(handle.get(), contents);
}

void CheckedCloseFile(FileHandle file) {
  CHECK(LoggingCloseFile(file));
}

}  // namespace crashpad
