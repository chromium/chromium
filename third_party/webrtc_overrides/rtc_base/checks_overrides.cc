// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "third_party/webrtc/rtc_base/checks.h"

// TODO(bugs.webrtc.org/42232595): Remove this once checks are moved to
// webrtc namespace.
#if defined(RTC_CHECKS_IN_WEBRTC_NAMESPACE)
namespace webrtc::webrtc_checks_impl {
#else
namespace rtc::webrtc_checks_impl {
#endif

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
}  // namespace webrtc::webrtc_checks_impl
