// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_WEBRTC_OVERRIDES_TEST_METRONOME_LIKE_TASK_QUEUE_TEST_H_
#define THIRD_PARTY_WEBRTC_OVERRIDES_TEST_METRONOME_LIKE_TASK_QUEUE_TEST_H_

#include <functional>
#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/task_queue/task_queue_factory.h"
#include "third_party/webrtc/rtc_base/system/rtc_export.h"
#include "third_party/webrtc/rtc_base/task_utils/to_queued_task.h"

namespace blink {

struct RTC_EXPORT MetronomeLikeTaskQueueTestParams {
  std::function<std::unique_ptr<webrtc::TaskQueueFactory>()> task_queue_factory;
  base::TimeDelta metronome_tick;
};

class RTC_EXPORT MetronomeLikeTaskQueueTest
    : public ::testing::TestWithParam<MetronomeLikeTaskQueueTestParams> {
 public:
  MetronomeLikeTaskQueueTest()
      : task_environment_(
            base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        task_queue_factory_(GetParam().task_queue_factory()),
        metronome_tick_(GetParam().metronome_tick) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<webrtc::TaskQueueFactory> task_queue_factory_;
  const base::TimeDelta metronome_tick_;
};

}  // namespace blink

#endif  // THIRD_PARTY_WEBRTC_OVERRIDES_TEST_METRONOME_LIKE_TASK_QUEUE_TEST_H_
