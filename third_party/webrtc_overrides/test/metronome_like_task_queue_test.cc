// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/test/metronome_like_task_queue_test.h"

#include <functional>
#include <memory>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/task_queue/queued_task.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"
#include "third_party/webrtc/rtc_base/task_utils/to_queued_task.h"

namespace blink {

using ::testing::ElementsAre;
using ::testing::MockFunction;

namespace {

std::unique_ptr<webrtc::QueuedTask> MockFunctionAsQueuedTask(
    MockFunction<void()>& mock_function) {
  return webrtc::ToQueuedTask(mock_function.AsStdFunction());
}

class MockCallback {
 public:
  MockCallback() {
    EXPECT_CALL(callback_, Call()).WillRepeatedly([&]() { ++callback_count_; });
  }

  size_t callback_count() const { return callback_count_; }
  bool was_called() const { return callback_count_ > 0; }

  std::unique_ptr<webrtc::QueuedTask> ToQueuedTask() {
    return MockFunctionAsQueuedTask(callback_);
  }

 private:
  MockFunction<void()> callback_;
  size_t callback_count_ = 0;
};

}  // namespace

TEST_P(MetronomeLikeTaskQueueTest, PostTaskRunsPriorToTick) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  MockCallback callback;
  task_queue->PostTask(callback.ToQueuedTask());

  EXPECT_FALSE(callback.was_called());
  task_environment_.FastForwardBy(base::Nanoseconds(1));
  EXPECT_TRUE(callback.was_called());
}

TEST_P(MetronomeLikeTaskQueueTest, NormalPriorityDelayedTasksRunOnTicks) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  // Delay task until next tick.
  MockCallback callback;
  task_queue->PostDelayedTask(callback.ToQueuedTask(),
                              metronome_tick_.InMilliseconds());
  EXPECT_EQ(callback.callback_count(), 0u);
  task_environment_.FastForwardBy(metronome_tick_);
  EXPECT_EQ(callback.callback_count(), 1u);

  // Delay half a tick. A full tick must pass before it runs.
  task_queue->PostDelayedTask(callback.ToQueuedTask(),
                              (metronome_tick_ / 2).InMilliseconds());
  task_environment_.FastForwardBy(metronome_tick_ - base::Milliseconds(1));
  EXPECT_EQ(callback.callback_count(), 1u);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(callback.callback_count(), 2u);

  // Delay several ticks.
  task_queue->PostDelayedTask(callback.ToQueuedTask(),
                              (metronome_tick_ * 3).InMilliseconds());
  task_environment_.FastForwardBy(metronome_tick_ * 2);
  EXPECT_EQ(callback.callback_count(), 2u);
  task_environment_.FastForwardBy(metronome_tick_);
  EXPECT_EQ(callback.callback_count(), 3u);
}

TEST_P(MetronomeLikeTaskQueueTest,
       NormalPriorityHighPrecisionDelayedTasksRunOutsideTicks) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  MockCallback callback;
  task_queue->PostDelayedHighPrecisionTask(callback.ToQueuedTask(),
                                           /*milliseconds=*/1);

  EXPECT_FALSE(callback.was_called());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(callback.was_called());
}

TEST_P(MetronomeLikeTaskQueueTest, CanDeleteTaskQueueWhileTasksAreInFlight) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  MockCallback callback;
  task_queue->PostTask(callback.ToQueuedTask());
  MockFunction<void()> low_precision_callback;
  EXPECT_CALL(low_precision_callback, Call()).Times(0);
  task_queue->PostDelayedTask(MockFunctionAsQueuedTask(low_precision_callback),
                              /*milliseconds=*/1);
  MockFunction<void()> high_precision_callback;
  EXPECT_CALL(high_precision_callback, Call()).Times(0);
  task_queue->PostDelayedHighPrecisionTask(
      MockFunctionAsQueuedTask(high_precision_callback),
      /*milliseconds=*/1);

  EXPECT_FALSE(callback.was_called());
  task_queue.reset();
  // The PostTask is executed, but not the delayed tasks.
  EXPECT_TRUE(callback.was_called());
  // Advance a tick. If the delayed tasks had not been cancelled, this would
  // trigger them to run. Our EXPECT_CALLs ensure us that they do not run.
  task_environment_.FastForwardBy(metronome_tick_);
}

TEST_P(MetronomeLikeTaskQueueTest, DelayedTasksRunInOrder) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  constexpr uint32_t kTime0Ms = 0;
  constexpr uint32_t kTime1Ms = 1;
  std::vector<std::string> run_tasks;

  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time0_First");
                              }),
                              kTime0Ms);
  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time1_First");
                              }),
                              kTime1Ms);
  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time1_Second");
                              }),
                              kTime1Ms);
  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time0_Second");
                              }),
                              kTime0Ms);
  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time0_Third");
                              }),
                              kTime0Ms);
  task_queue->PostDelayedTask(webrtc::ToQueuedTask([&run_tasks]() {
                                run_tasks.emplace_back("Time1_Third");
                              }),
                              kTime1Ms);
  task_environment_.FastForwardBy(metronome_tick_);

  EXPECT_THAT(run_tasks,
              ElementsAre("Time0_First", "Time0_Second", "Time0_Third",
                          "Time1_First", "Time1_Second", "Time1_Third"));
}

TEST_P(MetronomeLikeTaskQueueTest, DelayedTaskCanPostDelayedTask) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  bool task0_ran = false;
  bool task1_ran = false;

  task_queue->PostDelayedTask(
      webrtc::ToQueuedTask(
          [tick = metronome_tick_, &task_queue, &task0_ran, &task1_ran]() {
            task0_ran = true;
            // Inception!
            task_queue->PostDelayedTask(
                webrtc::ToQueuedTask([&task1_ran]() { task1_ran = true; }),
                tick.InMilliseconds());
          }),
      metronome_tick_.InMilliseconds());

  task_environment_.FastForwardBy(metronome_tick_);
  EXPECT_TRUE(task0_ran);
  EXPECT_FALSE(task1_ran);
  task_environment_.FastForwardBy(metronome_tick_);
  EXPECT_TRUE(task0_ran);
  EXPECT_TRUE(task1_ran);
}

}  // namespace blink
