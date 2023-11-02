// Copyright 2021 The Crashpad Authors
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

#include "util/ios/raw_logging.h"

#include <atomic>

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"

namespace crashpad {
namespace internal {

namespace {
static_assert(std::atomic<FileHandle>::is_always_lock_free,
              "std::atomic<FileHandle> may not be signal-safe");
std::atomic<FileHandle> g_file_handle = STDERR_FILENO;
}  // namespace

void SetFileHandleForTesting(FileHandle file_handle) {
  g_file_handle = file_handle;
}

void RawLogString(const char* message) {
  const size_t message_len = strlen(message);
  size_t bytes_written = 0;
  while (bytes_written < message_len) {
    int rv = HANDLE_EINTR(write(
        g_file_handle, message + bytes_written, message_len - bytes_written));
    if (rv < 0) {
      // Give up, nothing we can do now.
      break;
    }
    bytes_written += rv;
  }
}

void RawLogInt(unsigned int number) {
  char buffer[20];
  char* digit = &buffer[sizeof(buffer) - 1];
  *digit = '\0';
  do {
    *(--digit) = (number % 10) + '0';
    number /= 10;
  } while (number != 0);
  RawLogString(digit);
}

// Prints `path:linenum message:error` (with optional `:error`).
void RawLog(const char* file, int line, const char* message, int error) {
  RawLogString(file);
  HANDLE_EINTR(write(g_file_handle, ":", 1));
  RawLogInt(line);
  HANDLE_EINTR(write(g_file_handle, " ", 1));
  RawLogString(message);
  if (error) {
    RawLogString(": ");
    RawLogInt(error);
  }
  HANDLE_EINTR(write(g_file_handle, "\n", 1));
}

}  // namespace internal
}  // namespace crashpad
