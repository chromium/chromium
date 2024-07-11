// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc/rtc_base/checks.h"

#include "base/logging.h"

namespace rtc::webrtc_checks_impl {

RTC_NORETURN void WriteFatalLog(std::string_view output) {
  LOG(FATAL) << output;
  __builtin_unreachable();
}

RTC_NORETURN void WriteFatalLog(const char* file,
                                int line,
                                std::string_view output) {
  {
    logging::LogMessage msg(file, line, logging::LOGGING_FATAL);
    msg.stream() << output;
  }
  __builtin_unreachable();
}

}  // namespace rtc::webrtc_checks_impl
