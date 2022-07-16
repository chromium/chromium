// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/fraction_of_time_without_user_input_recorder.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"

namespace ui {

namespace {

using base::Bucket;
using testing::ElementsAre;
using testing::Pair;

const char* kHistogram = "Event.FractionOfTimeWithoutUserInput";

class TestFractionOfTimeWithoutUserInputRecorder
    : public FractionOfTimeWithoutUserInputRecorder {
 public:
  void RecordActiveInterval(base::TimeTicks start_time,
                            base::TimeTicks end_time) override {
    active_intervals_.push_back(std::make_pair(start_time, end_time));
    FractionOfTimeWithoutUserInputRecorder::RecordActiveInterval(start_time,
                                                                 end_time);
  }

  void set_window_size_for_test(base::TimeDelta window_size) {
    set_window_size(window_size);
  }

  void set_idle_timeout_for_test(base::TimeDelta idle_timeout) {
    set_idle_timeout(idle_timeout);
  }

  const std::vector<std::pair<base::TimeTicks, base::TimeTicks>>&
  active_intervals() {
    return active_intervals_;
  }

 private:
  std::vector<std::pair<base::TimeTicks, base::TimeTicks>> active_intervals_;
};

TEST(FractionOfTimeWithoutUserInputRecorderTest, IntervalIncludesIdleTimeout) {
  TestFractionOfTimeWithoutUserInputRecorder idle_fraction_recorder;
  idle_fraction_recorder.set_idle_timeout_for_test(base::Seconds(0.1));

  idle_fraction_recorder.RecordEventAtTime(EventTimeStampFromSeconds(0.5));

  // Flush the previous event.
  idle_fraction_recorder.RecordEventAtTime(EventTimeStampFromSeconds(100));

  // We observed a single event, so the we consider it to have lasted a duration
  // of one idle timeout.
  EXPECT_THAT(idle_fraction_recorder.active_intervals(),
              ElementsAre(Pair(EventTimeStampFromSeconds(0.5),
                               EventTimeStampFromSeconds(0.6))));
}

TEST(FractionOfTimeWithoutUserInputRecorderTest, TwoLongIntervals) {
  TestFractionOfTimeWithoutUserInputRecorder idle_fraction_recorder;
  idle_fraction_recorder.set_idle_timeout_for_test(base::Seconds(0.1));

  // Send events regularly between 0.1 seconds and 0.1 + 20 * 0.05 = 1.10
  // seconds.
  base::TimeTicks time = base::TimeTicks() + base::Seconds(0.1);
  idle_fraction_recorder.RecordEventAtTime(time);

  for (int i = 0; i < 20; ++i) {
    time += base::Seconds(0.05);
    idle_fraction_recorder.RecordEventAtTime(time);
  }

  // Send events regularly between 2.2 seconds and 2.2 + 20 * 0.05 = 3.20
  // seconds.
  time = base::TimeTicks() + base::Seconds(2.2);
  idle_fraction_recorder.RecordEventAtTime(time);

  for (int i = 0; i < 20; ++i) {
    time += base::Seconds(0.05);
    idle_fraction_recorder.RecordEventAtTime(time);
  }

  // Flush the previous event.
  idle_fraction_recorder.RecordEventAtTime(EventTimeStampFromSeconds(100));

  // Interval end times include idle timeout.
  EXPECT_THAT(idle_fraction_recorder.active_intervals(),
              ElementsAre(Pair(EventTimeStampFromSeconds(0.1),
                               EventTimeStampFromSeconds(1.2)),
                          Pair(EventTimeStampFromSeconds(2.2),
                               EventTimeStampFromSeconds(3.3))));
}

TEST(FractionOfTimeWithoutUserInputRecorderTest, SingleShortRange) {
  TestFractionOfTimeWithoutUserInputRecorder idle_fraction_recorder;
  idle_fraction_recorder.set_window_size_for_test(base::Seconds(1));

  base::HistogramTester tester;
  // Start window at 1 second.
  idle_fraction_recorder.RecordActiveInterval(EventTimeStampFromSeconds(1),
                                              EventTimeStampFromSeconds(1));

  idle_fraction_recorder.RecordActiveInterval(EventTimeStampFromSeconds(1.1),
                                              EventTimeStampFromSeconds(1.6));

  // Flush the previous interval.
  idle_fraction_recorder.RecordActiveInterval(EventTimeStampFromSeconds(2),
                                              EventTimeStampFromSeconds(2));

  EXPECT_THAT(tester.GetAllSamples(kHistogram), ElementsAre(Bucket(50, 1)));
}

TEST(FractionOfTimeWithoutUserInputRecorderTest, SingleLongRange) {
  TestFractionOfTimeWithoutUserInputRecorder idle_fraction_recorder;
  idle_fraction_recorder.set_window_size_for_test(base::Seconds(1));

  base::HistogramTester tester;

  // Start window at 1 second.
  idle_fraction_recorder.RecordActiveInterval(EventTimeStampFromSeconds(1),
                                              EventTimeStampFromSeconds(1));

  idle_fraction_recorder.RecordActiveInterval(EventTimeStampFromSeconds(1.1),
                                              EventTimeStampFromSeconds(4.2));

  // Flush the previous interval.
  idle_fraction_recorder.RecordActiveInterval(EventTimeStampFromSeconds(5),
                                              EventTimeStampFromSeconds(5));

  // The windows contain: [1.1, 2], [2, 3], [3, 4], [4, 4.2].
  EXPECT_THAT(tester.GetAllSamples(kHistogram),
              ElementsAre(Bucket(0, 2), Bucket(10, 1), Bucket(80, 1)));
}

TEST(FractionOfTimeWithoutUserInputRecorderTest, TwoLongRanges) {
  TestFractionOfTimeWithoutUserInputRecorder idle_fraction_recorder;
  idle_fraction_recorder.set_window_size_for_test(base::Seconds(1));

  base::HistogramTester tester;

  // Start window at 1 second.
  idle_fraction_recorder.RecordActiveInterval(EventTimeStampFromSeconds(1),
                                              EventTimeStampFromSeconds(1));

  idle_fraction_recorder.RecordActiveInterval(EventTimeStampFromSeconds(1.1),
                                              EventTimeStampFromSeconds(2.2));

  idle_fraction_recorder.RecordActiveInterval(EventTimeStampFromSeconds(2.6),
                                              EventTimeStampFromSeconds(3.2));

  // Flush the previous interval.
  idle_fraction_recorder.RecordActiveInterval(EventTimeStampFromSeconds(4),
                                              EventTimeStampFromSeconds(4));

  // The windows contain:
  // 1: 1.1 - 2.0
  // 2: 2.0 - 2.2, 2.6 - 3.0
  // 3: 3.0 - 3.2
  EXPECT_THAT(tester.GetAllSamples(kHistogram),
              ElementsAre(Bucket(10, 1), Bucket(40, 1), Bucket(80, 1)));
}

}  // namespace

}  // namespace ui
