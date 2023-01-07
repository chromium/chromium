// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/animation_runner.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {
namespace {

using AnimationRunnerTest = testing::Test;

// Verifies that calling Stop() during Step() actually stops the timer.
TEST(AnimationRunnerTest, StopDuringStep) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  auto runner = AnimationRunner::CreateDefaultAnimationRunner();
  constexpr auto kDelay = base::Milliseconds(20);
  int call_count = 0;
  runner->Start(kDelay, base::TimeDelta(),
                base::BindLambdaForTesting([&](base::TimeTicks ticks) {
                  ++call_count;
                  runner->Stop();
                }));
  EXPECT_EQ(0, call_count);
  task_environment.FastForwardBy(kDelay);
  EXPECT_EQ(1, call_count);
  task_environment.FastForwardBy(kDelay);
  EXPECT_EQ(1, call_count);
}

}  // namespace
}  // namespace gfx
