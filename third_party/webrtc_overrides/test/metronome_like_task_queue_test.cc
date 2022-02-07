// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/test/metronome_like_task_queue_test.h"

#include <functional>
#include <memory>

#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"

namespace blink {

using ::testing::ElementsAre;

TEST_P(MetronomeLikeTaskQueueTest, PostTaskRunsPriorToTick) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  bool did_run = false;
  task_queue->PostTask(webrtc::ToQueuedTask([&did_run]() { did_run = true; }));

  EXPECT_FALSE(did_run);
  task_environment_.FastForwardBy(base::Nanoseconds(1));
  EXPECT_TRUE(did_run);
}

TEST_P(MetronomeLikeTaskQueueTest, NormalPriorityDelayedTasksRunOnTicks) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  int task_run_counter = 0;

  // Delay task until next tick.
  task_queue->PostDelayedTask(
      webrtc::ToQueuedTask([&task_run_counter]() { ++task_run_counter; }),
      metronome_tick_.InMilliseconds());
  EXPECT_EQ(task_run_counter, 0);
  task_environment_.FastForwardBy(metronome_tick_);
  EXPECT_EQ(task_run_counter, 1);

  // Delay half a tick. A full tick must pass before it runs.
  task_queue->PostDelayedTask(
      webrtc::ToQueuedTask([&task_run_counter]() { ++task_run_counter; }),
      (metronome_tick_ / 2).InMilliseconds());
  task_environment_.FastForwardBy(metronome_tick_ - base::Milliseconds(1));
  EXPECT_EQ(task_run_counter, 1);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_EQ(task_run_counter, 2);

  // Delay several ticks.
  task_queue->PostDelayedTask(
      webrtc::ToQueuedTask([&task_run_counter]() { ++task_run_counter; }),
      (metronome_tick_ * 3).InMilliseconds());
  task_environment_.FastForwardBy(metronome_tick_ * 2);
  EXPECT_EQ(task_run_counter, 2);
  task_environment_.FastForwardBy(metronome_tick_);
  EXPECT_EQ(task_run_counter, 3);
}

TEST_P(MetronomeLikeTaskQueueTest,
       NormalPriorityHighPrecisionDelayedTasksRunOutsideTicks) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  bool did_run = false;
  task_queue->PostDelayedHighPrecisionTask(
      webrtc::ToQueuedTask([&did_run]() { did_run = true; }),
      /*milliseconds=*/1);

  EXPECT_FALSE(did_run);
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(did_run);
}

TEST_P(MetronomeLikeTaskQueueTest, CanDeleteTaskQueueWhileTasksAreInFlight) {
  auto task_queue = task_queue_factory_->CreateTaskQueue(
      "MetronomeTestQueue", webrtc::TaskQueueFactory::Priority::NORMAL);

  bool did_run_post_task = false;
  task_queue->PostTask(webrtc::ToQueuedTask(
      [&did_run_post_task]() { did_run_post_task = true; }));
  bool did_run_low_precision_delay_task = false;
  task_queue->PostDelayedTask(
      webrtc::ToQueuedTask([&did_run_low_precision_delay_task]() {
        did_run_low_precision_delay_task = true;
      }),
      /*milliseconds=*/1);
  bool did_run_high_precision_delay_task = false;
  task_queue->PostDelayedHighPrecisionTask(
      webrtc::ToQueuedTask([&did_run_high_precision_delay_task]() {
        did_run_high_precision_delay_task = true;
      }),
      /*milliseconds=*/1);

  EXPECT_FALSE(did_run_post_task);
  EXPECT_FALSE(did_run_low_precision_delay_task);
  EXPECT_FALSE(did_run_high_precision_delay_task);
  task_queue.reset();

  // The PostTask is executed, but not the delayed tasks.
  EXPECT_TRUE(did_run_post_task);
  EXPECT_FALSE(did_run_low_precision_delay_task);
  EXPECT_FALSE(did_run_high_precision_delay_task);

  // The delayed tasks never run because they have been cancelled.
  task_environment_.FastForwardBy(metronome_tick_);
  EXPECT_FALSE(did_run_low_precision_delay_task);
  EXPECT_FALSE(did_run_high_precision_delay_task);
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
