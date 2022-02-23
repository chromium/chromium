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
  auto* task_queue = provider_->TaskQueue();

  MockCallback callback;
  EXPECT_FALSE(callback.was_called());
  task_queue->PostTask(callback.ToQueuedTask());

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
  task_queue->PostDelayedTask(callback.ToQueuedTask(),
                              provider_->MetronomeTick().InMilliseconds());
  EXPECT_EQ(callback.callback_count(), 0u);
  task_environment_.FastForwardBy(provider_->MetronomeTick());
  EXPECT_EQ(callback.callback_count(), 1u);

  // Delay half a tick. A full tick must pass before it runs.
  task_queue->PostDelayedTask(
      callback.ToQueuedTask(),
      (provider_->MetronomeTick() / 2).InMilliseconds());
  task_environment_.FastForwardBy(provider_->MetronomeTick() -
                                  base::Milliseconds(1));
  EXPECT_EQ(callback.callback_count(), 1u);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(callback.callback_count(), 2u);

  // Delay several ticks.
  task_queue->PostDelayedTask(
      callback.ToQueuedTask(),
      (provider_->MetronomeTick() * 3).InMilliseconds());
  task_environment_.FastForwardBy(provider_->MetronomeTick() * 2);
  EXPECT_EQ(callback.callback_count(), 2u);
  task_environment_.FastForwardBy(provider_->MetronomeTick());
  EXPECT_EQ(callback.callback_count(), 3u);
}

TEST_P(MetronomeLikeTaskQueueTest,
       NormalPriorityHighPrecisionDelayedTasksRunOutsideTicks) {
  auto* task_queue = provider_->TaskQueue();

  MockCallback callback;
  task_queue->PostDelayedHighPrecisionTask(callback.ToQueuedTask(),
                                           /*milliseconds=*/1);

  EXPECT_FALSE(callback.was_called());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(callback.was_called());
}

TEST_P(MetronomeLikeTaskQueueTest, DelayedTasksRunInOrder) {
  auto* task_queue = provider_->TaskQueue();

  constexpr uint32_t kTime0Ms = 1;
  constexpr uint32_t kTime1Ms = 2;
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
      webrtc::ToQueuedTask(
          [tick = provider_->MetronomeTick(), &task_queue, &task0_ran,
           &task1_ran]() {
            task0_ran = true;
            // Inception!
            task_queue->PostDelayedTask(
                webrtc::ToQueuedTask([&task1_ran]() { task1_ran = true; }),
                tick.InMilliseconds());
          }),
      provider_->MetronomeTick().InMilliseconds());

  task_environment_.FastForwardBy(provider_->MetronomeTick());
  EXPECT_TRUE(task0_ran);
  EXPECT_FALSE(task1_ran);
  task_environment_.FastForwardBy(provider_->MetronomeTick());
  EXPECT_TRUE(task0_ran);
  EXPECT_TRUE(task1_ran);
}

}  // namespace blink
