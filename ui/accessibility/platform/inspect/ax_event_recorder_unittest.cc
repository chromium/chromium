// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_event_recorder.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/simple_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

using ::testing::ElementsAre;

class TestAXEventRecorder : public AXEventRecorder {
 public:
  using AXEventRecorder::OnEvent;
};

class EventSenderThread : public base::SimpleThread {
 public:
  EventSenderThread(TestAXEventRecorder* recorder, int count)
      : base::SimpleThread("EventSenderThread"),
        recorder_(recorder),
        count_(count) {}

  void Run() override {
    for (int i = 0; i < count_; ++i) {
      recorder_->OnEvent("event");
    }
  }

 private:
  raw_ptr<TestAXEventRecorder> recorder_;
  const int count_;
};

TEST(AXEventRecorderTest, StopListeningToEventsFromCallback) {
  TestAXEventRecorder recorder;
  int callback_count = 0;
  std::vector<std::string> logs_seen_in_callback;

  recorder.ListenToEvents(base::BindRepeating(
      [](TestAXEventRecorder* recorder, int* callback_count,
         std::vector<std::string>* logs_seen_in_callback,
         const std::string& event) {
        ++(*callback_count);
        *logs_seen_in_callback = recorder->GetEventLogs();
        recorder->StopListeningToEvents();
      },
      &recorder, &callback_count, &logs_seen_in_callback));

  recorder.OnEvent("first");
  recorder.OnEvent("second");

  EXPECT_EQ(callback_count, 1);
  EXPECT_THAT(logs_seen_in_callback, ElementsAre("first"));
  EXPECT_THAT(recorder.GetEventLogs(), ElementsAre("first", "second"));
}

TEST(AXEventRecorderTest, ListenToEventsFromCallback) {
  TestAXEventRecorder recorder;
  std::vector<std::string> first_callback_events;
  std::vector<std::string> second_callback_events;

  recorder.ListenToEvents(base::BindRepeating(
      [](TestAXEventRecorder* recorder,
         std::vector<std::string>* first_callback_events,
         std::vector<std::string>* second_callback_events,
         const std::string& event) {
        first_callback_events->push_back(event);
        recorder->ListenToEvents(base::BindRepeating(
            [](std::vector<std::string>* second_callback_events,
               const std::string& next_event) {
              second_callback_events->push_back(next_event);
            },
            second_callback_events));
      },
      &recorder, &first_callback_events, &second_callback_events));

  recorder.OnEvent("first");
  recorder.OnEvent("second");

  EXPECT_THAT(first_callback_events, ElementsAre("first"));
  EXPECT_THAT(second_callback_events, ElementsAre("second"));
  EXPECT_THAT(recorder.GetEventLogs(), ElementsAre("first", "second"));
}

TEST(AXEventRecorderTest, ConcurrentOnEventAndCallbackUpdates) {
  constexpr int kThreadCount = 4;
  constexpr int kEventsPerThread = 200;

  TestAXEventRecorder recorder;
  std::atomic<int> callback_invocations{0};

  std::vector<std::unique_ptr<EventSenderThread>> threads;
  threads.reserve(kThreadCount);
  for (int i = 0; i < kThreadCount; ++i) {
    threads.push_back(
        std::make_unique<EventSenderThread>(&recorder, kEventsPerThread));
    threads.back()->Start();
  }

  for (int i = 0; i < 200; ++i) {
    recorder.ListenToEvents(base::BindRepeating(
        [](TestAXEventRecorder* recorder,
           std::atomic<int>* callback_invocations, const std::string& event) {
          callback_invocations->fetch_add(1, std::memory_order_relaxed);
          recorder->StopListeningToEvents();
        },
        &recorder, &callback_invocations));
    std::vector<std::string> snapshot = recorder.GetEventLogs();
    EXPECT_LE(snapshot.size(),
              static_cast<size_t>(kThreadCount * kEventsPerThread));
    recorder.ListenToEvents(base::DoNothing());
    recorder.StopListeningToEvents();
  }

  for (auto& thread : threads) {
    thread->Join();
  }

  EXPECT_EQ(recorder.GetEventLogs().size(),
            static_cast<size_t>(kThreadCount * kEventsPerThread));
}

}  // namespace
}  // namespace ui
