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

#include "gtest/gtest.h"
#include "util/misc/metrics.h"

namespace crashpad::test {
namespace {

using Metrics::CrashSkippedReason::kUnexpectedTime;
using Metrics::CrashSkippedReason::kUploadThrottled;

constexpr time_t kOneHour = 60 * 60;

TEST(CrashReportUploadRateLimitTest, ShouldRateLimit_OnePerHour) {
  const time_t now = time(nullptr);

  // Allow upload if the last upload attempt time is in the future, at or beyond
  // `kBackwardsClockTolerance`.
  auto should_rate_limit =
      ShouldRateLimit(now, now + kBackwardsClockTolerance + 1, kOneHour);
  EXPECT_EQ(should_rate_limit.skip_reason, std::nullopt);

  should_rate_limit =
      ShouldRateLimit(now, now + kBackwardsClockTolerance, kOneHour);
  EXPECT_EQ(should_rate_limit.skip_reason, std::nullopt);

  // Skip upload if the last attempt time is in the future but within
  // `kBackwardsClockTolerance`.
  should_rate_limit =
      ShouldRateLimit(now, now + kBackwardsClockTolerance - 1, kOneHour);
  EXPECT_EQ(should_rate_limit.skip_reason, kUnexpectedTime);

  should_rate_limit = ShouldRateLimit(now, now + 1, kOneHour);
  EXPECT_EQ(should_rate_limit.skip_reason, kUnexpectedTime);

  // Skip upload if the last attempt was under one interval ago.
  should_rate_limit = ShouldRateLimit(now, now, kOneHour);
  EXPECT_EQ(should_rate_limit.skip_reason, kUploadThrottled);

  should_rate_limit = ShouldRateLimit(now, now - 1, kOneHour);
  EXPECT_EQ(should_rate_limit.skip_reason, kUploadThrottled);

  should_rate_limit = ShouldRateLimit(now, now - kOneHour + 1, kOneHour);
  EXPECT_EQ(should_rate_limit.skip_reason, kUploadThrottled);

  // Allow upload if the last attempt was one interval or more ago.
  should_rate_limit = ShouldRateLimit(now, now - kOneHour, kOneHour);
  EXPECT_EQ(should_rate_limit.skip_reason, std::nullopt);

  should_rate_limit = ShouldRateLimit(now, now - kOneHour - 1, kOneHour);
  EXPECT_EQ(should_rate_limit.skip_reason, std::nullopt);

  should_rate_limit = ShouldRateLimit(now, 0, kOneHour);
  EXPECT_EQ(should_rate_limit.skip_reason, std::nullopt);
}

}  // namespace
}  // namespace crashpad::test
