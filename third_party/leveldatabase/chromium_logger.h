// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LEVELDATABASE_CHROMIUM_LOGGER_H_
#define THIRD_PARTY_LEVELDATABASE_CHROMIUM_LOGGER_H_

#include <cstdarg>
#include <utility>

#include "base/files/file.h"
#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

namespace leveldb {

class ChromiumLogger : public Logger {
 public:
  explicit ChromiumLogger(base::File file) : file_(std::move(file)) {}

  ~ChromiumLogger() override = default;

  void Logv(const char* format, va_list arguments) override {
    // Record the time as close to the Logv() call as possible.
    base::Time::Exploded now_exploded;
    base::Time::Now().LocalExplode(&now_exploded);

    const base::PlatformThreadId thread_id = base::PlatformThread::CurrentId();

    // We first attempt to print into a stack-allocated buffer. If this attempt
    // fails, we make a second attempt with a dynamically allocated buffer.
    constexpr const int kStackBufferSize = 512;
    char stack_buffer[kStackBufferSize];
    static_assert(sizeof(stack_buffer) == static_cast<size_t>(kStackBufferSize),
                  "sizeof(char) is expected to be 1 in C++");

    int dynamic_buffer_size = 0;  // Computed in the first iteration.
    for (int iteration = 0; iteration < 2; ++iteration) {
      const int buffer_size =
          (iteration == 0) ? kStackBufferSize : dynamic_buffer_size;
      char* const buffer =
          (iteration == 0) ? stack_buffer : new char[dynamic_buffer_size];

      // Print the header into the buffer.
      int buffer_offset = base::snprintf(
          buffer, buffer_size,
          "%04d/%02d/%02d-%02d:%02d:%02d.%03d %" PRIx64 " ",
          now_exploded.year,
          now_exploded.month,
          now_exploded.day_of_month,
          now_exploded.hour,
          now_exploded.minute,
          now_exploded.second,
          now_exploded.millisecond,
          static_cast<uint64_t>(thread_id));

      // The header can be at most 45 characters (10 date + 12 time + 3 spacing
      // + 20 thread ID), which should fit comfortably into the static buffer.
      DCHECK_LE(buffer_offset, 45);
      static_assert(45 < kStackBufferSize,
                    "stack-allocated buffer may not fit the message header");
      DCHECK_LT(buffer_offset, buffer_size);

      // Print the message into the buffer.
      std::va_list arguments_copy;
      va_copy(arguments_copy, arguments);
      buffer_offset += std::vsnprintf(buffer + buffer_offset,
                                      buffer_size - buffer_offset, format,
                                      arguments_copy);
      va_end(arguments_copy);

      // The code below may append a newline at the end of the buffer, which
      // requires an extra character.
      if (buffer_offset >= buffer_size - 1) {
        // The message did not fit into the buffer.
        if (iteration == 0) {
          // Re-run the loop and use a dynamically-allocated buffer. The buffer
          // will be large enough for the log message, an extra newline and a
          // null terminator.
          dynamic_buffer_size = buffer_offset + 2;
          continue;
        }

        // The dynamically-allocated buffer was incorrectly sized. This should
        // not happen, assuming a correct implementation of (v)snprintf. Fail
        // in tests, recover by truncating the log message in production.
        NOTREACHED();
        buffer_offset = buffer_size - 1;
      }

      // Add a newline if necessary.
      if (buffer[buffer_offset - 1] != '\n') {
        buffer[buffer_offset] = '\n';
        ++buffer_offset;
      }

      DCHECK_LE(buffer_offset, buffer_size);
      file_.WriteAtCurrentPos(buffer, buffer_offset);

      if (iteration != 0) {
        delete[] buffer;
      }
      break;
    }
  }

 private:
  base::File file_;
};

}  // namespace leveldb

#endif  // THIRD_PARTY_LEVELDATABASE_CHROMIUM_LOGGER_H_
