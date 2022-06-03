// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "util/stream/log_output_stream.h"

#include <string.h>

#include <algorithm>

#include "base/check.h"
#include "base/logging.h"

namespace crashpad {

LogOutputStream::LogOutputStream(std::unique_ptr<Delegate> delegate)
    : delegate_(std::move(delegate)),
      output_count_(0),
      flush_needed_(false),
      flushed_(false) {
  buffer_.reserve(delegate_->LineWidth());
}

LogOutputStream::~LogOutputStream() {
  DCHECK(!flush_needed_);
}

bool LogOutputStream::Write(const uint8_t* data, size_t size) {
  DCHECK(!flushed_);

  static constexpr char kBeginMessage[] = "-----BEGIN CRASHPAD MINIDUMP-----";
  if (output_count_ == 0 && WriteToLog(kBeginMessage) < 0) {
    return false;
  }

  flush_needed_ = true;
  while (size > 0) {
    size_t m = std::min(delegate_->LineWidth() - buffer_.size(), size);
    buffer_.append(reinterpret_cast<const char*>(data), m);
    data += m;
    size -= m;
    if (buffer_.size() == delegate_->LineWidth() && !WriteBuffer()) {
      return false;
    }
  }
  return true;
}

bool LogOutputStream::WriteBuffer() {
  if (buffer_.empty())
    return true;

  static constexpr char kAbortMessage[] = "-----ABORT CRASHPAD MINIDUMP-----";

  output_count_ += buffer_.size();
  if (output_count_ > delegate_->OutputCap()) {
    WriteToLog(kAbortMessage);
    flush_needed_ = false;
    return false;
  }

  int result = WriteToLog(buffer_.c_str());
  if (result < 0) {
    if (result == -EAGAIN) {
      WriteToLog(kAbortMessage);
    }
    flush_needed_ = false;
    return false;
  }

  buffer_.clear();
  return true;
}

int LogOutputStream::WriteToLog(const char* buf) {
  return delegate_->Log(buf);
}

bool LogOutputStream::Flush() {
  bool result = true;
  if (flush_needed_) {
    flush_needed_ = false;
    flushed_ = true;

    static constexpr char kEndMessage[] = "-----END CRASHPAD MINIDUMP-----";
    if (!WriteBuffer() || WriteToLog(kEndMessage) < 0) {
      result = false;
    }
  }
  return result;
}

}  // namespace crashpad
