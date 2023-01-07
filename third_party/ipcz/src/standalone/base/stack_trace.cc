// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "standalone/base/stack_trace.h"

#include <string>
#include <vector>

#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/debugging/stacktrace.h"
#include "third_party/abseil-cpp/absl/debugging/symbolize.h"
#include "third_party/abseil-cpp/absl/strings/str_join.h"

namespace ipcz::standalone {

StackTrace::StackTrace(size_t frame_count) {
  frames_.resize(frame_count);
  int num_frames =
      absl::GetStackTrace(frames_.data(), static_cast<int>(frame_count), 1);
  ABSL_ASSERT(num_frames >= 0);
  frames_.resize(static_cast<size_t>(num_frames));
}

StackTrace::StackTrace(const StackTrace&) = default;

StackTrace& StackTrace::operator=(const StackTrace&) = default;

StackTrace::~StackTrace() = default;

// static
void StackTrace::EnableStackTraceSymbolization(const char* argv0) {
  absl::InitializeSymbolizer(argv0);
}

std::string StackTrace::ToString() const {
  std::vector<char> buffer;
  static constexpr size_t kMaxSymbolizedFrameLength = 256;
  for (void* frame : frames_) {
    char symbolized[kMaxSymbolizedFrameLength] = "unknown\n";
    absl::Symbolize(frame, symbolized, kMaxSymbolizedFrameLength);

    const size_t length = strlen(symbolized);
    const size_t index = buffer.size();
    buffer.resize(buffer.size() + length + 1);
    memcpy(&buffer[index], symbolized, length);
    buffer[index + length] = '\n';
  }
  return std::string(buffer.begin(), buffer.end());
}

}  // namespace ipcz::standalone
