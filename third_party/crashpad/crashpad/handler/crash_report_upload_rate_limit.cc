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

#include "handler/crash_report_upload_rate_limit.h"

#include <time.h>

#include <optional>

#include "util/misc/metrics.h"

namespace crashpad {

ShouldRateLimitResult ShouldRateLimit(time_t now,
                                      time_t last_upload_attempt_time,
                                      time_t interval_seconds) {
  if (now >= last_upload_attempt_time) {
    // If the most recent upload attempt occurred within the past interval,
    // don’t attempt to upload the new report. If it happened longer ago,
    // attempt to upload the report.
    if (now - last_upload_attempt_time < interval_seconds) {
      return {1, Metrics::CrashSkippedReason::kUploadThrottled};
    }
  } else if (last_upload_attempt_time - now < kBackwardsClockTolerance) {
    // The most recent upload attempt purportedly occurred in the future. If
    // it “happened” at least `kBackwardsClockTolerance` in the future, assume
    // that the last upload attempt time is bogus, and attempt to upload the
    // report. If the most recent upload time is in the future but within
    // `kBackwardsClockTolerance`, accept it and don’t attempt to upload the
    // report.
    return {1, Metrics::CrashSkippedReason::kUnexpectedTime};
  }
  return {0, std::nullopt};
}

}  // namespace crashpad
