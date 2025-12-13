// Copyright 2025 The Crashpad Authors
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

#ifndef CRASHPAD_HANDLER_CRASH_REPORT_UPLOAD_RATE_LIMIT_H_
#define CRASHPAD_HANDLER_CRASH_REPORT_UPLOAD_RATE_LIMIT_H_

#include <time.h>

#include <optional>

#include "util/misc/metrics.h"

namespace crashpad {

//! \brief How far into the future an upload-attempt timestamp can be and still
//!     be treated as valid. Beyond this tolerance, timestamps are disregarded.
//!     Exposed for testing.
inline constexpr int kBackwardsClockTolerance = 60 * 60 * 24;  // 1 day

//! \brief The return type of `ShouldRateLimit()`. Describes the number of
//!     crash-report upload attempts in the past interval, and if upload
//!     should be skipped, the reason why.
struct ShouldRateLimitResult {
  //! \brief The number of upload attempts in the past interval.
  unsigned int attempts_in_interval;

  //! \brief If the crash should be skipped, the reason why. `std::nullopt`
  //!     otherwise.
  std::optional<Metrics::CrashSkippedReason> skip_reason;
};

//! \brief Determines whether a crash report can be uploaded now, or whether
//!     uploads should be rate limited at this time.
//!
//! \param[in] now The current time.
//! \param[in] last_upload_attempt_time The timestamp of the last crash-report
//!     upload attempt.
//! \param[in] interval_seconds The unit of time, in seconds, over which to
//!     perform rate-limiting calculations (an "interval").
//!
//! \return The number of upload attempts in the past interval, and if the
//!     current crash report should be skipped, the reason why.
ShouldRateLimitResult ShouldRateLimit(time_t now,
                                      time_t last_upload_attempt_time,
                                      time_t interval_seconds);

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_CRASH_REPORT_UPLOAD_RATE_LIMIT_H_
