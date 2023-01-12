// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/repeat_controller.h"

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace views {

namespace {

class RepeatControllerTest : public testing::Test {
 public:
  RepeatControllerTest() = default;
  ~RepeatControllerTest() override = default;

  void SetUp() override {
    // Ensures that the callback hasn't fired at initialization.
    ASSERT_EQ(0, times_called_);
  }

 protected:
  // Short wait that must be below both
  // RepeatController::GetInitialWaitForTesting() and
  // RepeatController::GetRepeatingWaitForTesting().
  static constexpr base::TimeDelta kShortWait = base::Milliseconds(10);
  static_assert(
      kShortWait < RepeatController::GetInitialWaitForTesting(),
      "kShortWait must be shorter than the RepeatController initial wait.");
  static_assert(
      kShortWait < RepeatController::GetRepeatingWaitForTesting(),
      "kShortWait must be shorter than the RepeatController repeating wait.");

  int times_called() const { return times_called_; }
  RepeatController& repeat_controller() { return repeat_controller_; }

  void AdvanceTime(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
  }

 private:
  void IncrementTimesCalled() { ++times_called_; }

  int times_called_ = 0;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  RepeatController repeat_controller_{
      base::BindRepeating(&RepeatControllerTest::IncrementTimesCalled,
                          base::Unretained(this)),
      task_environment_.GetMockTickClock()};
};

// static
constexpr base::TimeDelta RepeatControllerTest::kShortWait;

}  // namespace

TEST_F(RepeatControllerTest, StartStop) {
  repeat_controller().Start();
  repeat_controller().Stop();
  EXPECT_EQ(0, times_called());
}

TEST_F(RepeatControllerTest, StartShortWait) {
  repeat_controller().Start();
  AdvanceTime(RepeatController::GetInitialWaitForTesting() - kShortWait);
  EXPECT_EQ(0, times_called());
  repeat_controller().Stop();
}

TEST_F(RepeatControllerTest, StartInitialWait) {
  repeat_controller().Start();
  AdvanceTime(RepeatController::GetInitialWaitForTesting() +
              RepeatController::GetRepeatingWaitForTesting() - kShortWait);
  EXPECT_EQ(1, times_called());
  repeat_controller().Stop();
}

TEST_F(RepeatControllerTest, StartLongerWait) {
  constexpr int kExpectedCallbacks = 34;
  repeat_controller().Start();
  AdvanceTime(RepeatController::GetInitialWaitForTesting() +
              (RepeatController::GetRepeatingWaitForTesting() *
               (kExpectedCallbacks - 1)) +
              kShortWait);
  EXPECT_EQ(kExpectedCallbacks, times_called());
  repeat_controller().Stop();
}

TEST_F(RepeatControllerTest, NoCallbacksAfterStop) {
  constexpr int kExpectedCallbacks = 34;
  repeat_controller().Start();
  AdvanceTime(RepeatController::GetInitialWaitForTesting() +
              (RepeatController::GetRepeatingWaitForTesting() *
               (kExpectedCallbacks - 1)) +
              kShortWait);
  repeat_controller().Stop();
  AdvanceTime(RepeatController::GetInitialWaitForTesting() +
              RepeatController::GetRepeatingWaitForTesting());
  EXPECT_EQ(kExpectedCallbacks, times_called());
}

}  // namespace views
