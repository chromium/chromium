// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/test/metronome_like_task_queue_test.h"

#include <functional>
#include <memory>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/functional/any_invocable.h"

namespace blink {

using ::testing::ElementsAre;
using ::testing::MockFunction;

namespace {

class MockCallback {
 public:
  MockCallback() {
    EXPECT_CALL(callback_, Call()).WillRepeatedly([&]() { ++callback_count_; });
  }

  size_t callback_count() const { return callback_count_; }
  bool was_called() const { return callback_count_ > 0; }

  absl::AnyInvocable<void() &&> ToTask() { return callback_.AsStdFunction(); }

 private:
  MockFunction<void()> callback_;
  size_t callback_count_ = 0;
};

webrtc::TimeDelta ToWebrtc(base::TimeDelta delta) {
  return webrtc::TimeDelta::Micros(delta.InMicroseconds());
}

}  // namespace

TEST_P(MetronomeLikeTaskQueueTest, PostTaskRunsPriorToTick) {
  auto* task_queue = provider_->TaskQueue();

  MockCallback callback;
  EXPECT_FALSE(callback.was_called());
  task_queue->PostTask(callback.ToTask());

  // The task environment uses multiple threads so it's possible for the
  // callback to be invoked as soon as we call PostTask(), but by advancing time
  // we ensure the task has had time to run.
  task_environment_.FastForwardBy(base::Nanoseconds(1));
  EXPECT_TRUE(callback.was_called());
}

TEST_P(MetronomeLikeTaskQueueTest, NormalPriorityDelayedTasksRunOnTicks) {
  auto* task_queue = provider_->TaskQueue();

  // Delay task until next tick.
  MockCallback callback;
  task_queue->PostDelayedTask(callback.ToTask(),
                              ToWebrtc(provider_->MetronomeTick()));
  EXPECT_EQ(callback.callback_count(), 0u);
  task_environment_.FastForwardBy(provider_->MetronomeTick());
  EXPECT_EQ(callback.callback_count(), 1u);

  // Delay half a tick. A full tick must pass before it runs.
  task_queue->PostDelayedTask(callback.ToTask(),
                              ToWebrtc(provider_->MetronomeTick() / 2));
  task_environment_.FastForwardBy(provider_->MetronomeTick() -
                                  base::Milliseconds(1));
  EXPECT_EQ(callback.callback_count(), 1u);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(callback.callback_count(), 2u);

  // Delay several ticks.
  task_queue->PostDelayedTask(callback.ToTask(),
                              ToWebrtc(provider_->MetronomeTick() * 3));
  task_environment_.FastForwardBy(provider_->MetronomeTick() * 2);
  EXPECT_EQ(callback.callback_count(), 2u);
  task_environment_.FastForwardBy(provider_->MetronomeTick());
  EXPECT_EQ(callback.callback_count(), 3u);
}

TEST_P(MetronomeLikeTaskQueueTest,
       NormalPriorityHighPrecisionDelayedTasksRunOutsideTicks) {
  auto* task_queue = provider_->TaskQueue();

  MockCallback callback;
  task_queue->PostDelayedHighPrecisionTask(callback.ToTask(),
                                           webrtc::TimeDelta::Millis(1));

  EXPECT_FALSE(callback.was_called());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(callback.was_called());
}

TEST_P(MetronomeLikeTaskQueueTest, MixedPrecisionDelayedTasksRunAsExpected) {
  auto* task_queue = provider_->TaskQueue();
  MockCallback callback;

  // Setup 3 callbacks:
  // 1) Low precision, should execute on the first metronome tick.
  // 2) High precision, should execute between first and second metronome tick.
  // 3) Low precision, should execute on the second metronome tick.
  const base::TimeDelta tick_duration = provider_->MetronomeTick();
  const webrtc::TimeDelta webrtc_tick_duration = ToWebrtc(tick_duration);
  task_queue->PostDelayedTask(callback.ToTask(), webrtc_tick_duration / 2);
  task_queue->PostDelayedHighPrecisionTask(callback.ToTask(),
                                           3 * webrtc_tick_duration / 2);
  task_queue->PostDelayedTask(callback.ToTask(), 3 * webrtc_tick_duration / 2);
  task_environment_.FastForwardBy(tick_duration);
  EXPECT_EQ(callback.callback_count(), 1u);
  task_environment_.FastForwardBy(tick_duration / 2 + base::Microseconds(1));
  EXPECT_EQ(callback.callback_count(), 2u);
  task_environment_.FastForwardBy(tick_duration / 2 + base::Microseconds(1));
  EXPECT_EQ(callback.callback_count(), 3u);
}

TEST_P(MetronomeLikeTaskQueueTest, DelayedTasksRunInOrder) {
  auto* task_queue = provider_->TaskQueue();

  constexpr webrtc::TimeDelta kTime0 = webrtc::TimeDelta::Millis(1);
  constexpr webrtc::TimeDelta kTime1 = webrtc::TimeDelta::Millis(2);
  std::vector<std::string> run_tasks;

  task_queue->PostDelayedTask(
      [&run_tasks]() { run_tasks.emplace_back("Time0_First"); }, kTime0);
  task_queue->PostDelayedTask(
      [&run_tasks]() { run_tasks.emplace_back("Time1_First"); }, kTime1);
  task_queue->PostDelayedTask(
      [&run_tasks]() { run_tasks.emplace_back("Time1_Second"); }, kTime1);
  task_queue->PostDelayedTask(
      [&run_tasks]() { run_tasks.emplace_back("Time0_Second"); }, kTime0);
  task_queue->PostDelayedTask(
      [&run_tasks]() { run_tasks.emplace_back("Time0_Third"); }, kTime0);
  task_queue->PostDelayedTask(
      [&run_tasks]() { run_tasks.emplace_back("Time1_Third"); }, kTime1);
  task_environment_.FastForwardBy(provider_->MetronomeTick());

  EXPECT_THAT(run_tasks,
              ElementsAre("Time0_First", "Time0_Second", "Time0_Third",
                          "Time1_First", "Time1_Second", "Time1_Third"));
}

TEST_P(MetronomeLikeTaskQueueTest, DelayedTaskCanPostDelayedTask) {
  auto* task_queue = provider_->TaskQueue();

  bool task0_ran = false;
  bool task1_ran = false;

  task_queue->PostDelayedTask(
      [tick = provider_->MetronomeTick(), &task_queue, &task0_ran,
       &task1_ran]() {
        task0_ran = true;
        // Inception!
        task_queue->PostDelayedTask([&task1_ran]() { task1_ran = true; },
                                    ToWebrtc(tick));
      },
      ToWebrtc(provider_->MetronomeTick()));

  task_environment_.FastForwardBy(provider_->MetronomeTick());
  EXPECT_TRUE(task0_ran);
  EXPECT_FALSE(task1_ran);
  task_environment_.FastForwardBy(provider_->MetronomeTick());
  EXPECT_TRUE(task0_ran);
  EXPECT_TRUE(task1_ran);
}

}  // namespace blink
