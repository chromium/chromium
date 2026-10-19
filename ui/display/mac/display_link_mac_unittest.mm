// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mac/display_link_mac.h"

#include <CoreGraphics/CoreGraphics.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_constants.h"

namespace ui {
namespace {

// The DisplayLinkMac implementation that DisplayLinkMac::GetForDisplay()
// returns, selected through feature flags.
enum class DisplayLinkKind {
  // Default: CVDisplayLinkMac.
  kCVDisplayLink,
  // CADisplayLinkMac created in the GPU process. Requires macOS 14.0+.
  kCADisplayLinkInGpu,
};

std::string DisplayLinkKindToString(
    const testing::TestParamInfo<DisplayLinkKind>& info) {
  switch (info.param) {
    case DisplayLinkKind::kCVDisplayLink:
      return "CVDisplayLink";
    case DisplayLinkKind::kCADisplayLinkInGpu:
      return "CADisplayLinkInGpu";
  }
}

// Exercises the real DisplayLinkMac implementations against the main display.
// These tests need a display and a WindowServer connection, so they skip when
// no main display is available.
class DisplayLinkMacTest : public testing::TestWithParam<DisplayLinkKind> {
 protected:
  void SetUp() override {
    switch (GetParam()) {
      case DisplayLinkKind::kCVDisplayLink:
        features_.InitAndDisableFeature(display::features::kCADisplayLinkInGpu);
        break;
      case DisplayLinkKind::kCADisplayLinkInGpu:
        features_.InitAndEnableFeature(display::features::kCADisplayLinkInGpu);
        break;
    }
  }

  // Returns false if the current platform cannot provide the requested kind.
  bool IsSupported() const {
    if (GetParam() == DisplayLinkKind::kCADisplayLinkInGpu) {
      if (@available(macOS 14.0, *)) {
        return true;
      }
      return false;
    }
    return true;
  }

  // CADisplayLink delivers callbacks through the CFRunLoop of the thread that
  // created it, and CVDisplayLink posts them back to the registering thread,
  // so the main thread needs a UI message pump.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::test::ScopedFeatureList features_;
};

// Verifies that a DisplayLinkMac can be created for the main display and that
// an invalid display ID is rejected.
TEST_P(DisplayLinkMacTest, DisplayLinkCreation) {
  if (!IsSupported()) {
    GTEST_SKIP() << "Not supported on this macOS version";
  }

  const CGDirectDisplayID display_id = CGMainDisplayID();
  if (display_id == kCGNullDirectDisplay) {
    GTEST_SKIP() << "No main display";
  }

  scoped_refptr<DisplayLinkMac> display_link =
      DisplayLinkMac::GetForDisplay(display_id);
  EXPECT_TRUE(display_link);

  // Negative IDs are internal sentinels such as display::kInvalidDisplayId
  // and must never produce a display link.
  EXPECT_FALSE(DisplayLinkMac::GetForDisplay(display::kInvalidDisplayId));
}

// Creates a DisplayLinkMac for the main display, registers a VSync callback,
// and collects a batch of callbacks. After skipping a few warm-up callbacks,
// verifies that the average interval between callbacks matches the interval
// reported in VSyncParamsMac, both for the times the callbacks arrive and for
// the timebases the display link reports. Then destroys the VSyncCallbackMac
// and verifies that no further callbacks arrive.
TEST_P(DisplayLinkMacTest, CallbackIntervalIsAccurate) {
  // Leading VSync callbacks that are excluded from the interval checks because
  // their timing includes the display link start-up latency.
  constexpr size_t kWarmUpCallbacks = 5;
  // Number of VSync callbacks after the warm-up that the intervals are
  // averaged over.
  constexpr size_t kMeasuredCallbacks = 20;
  constexpr size_t kCallbacksToCollect = kWarmUpCallbacks + kMeasuredCallbacks;
  // Maximum allowed difference between the average measured interval and the
  // reported interval. For callback arrival times this is a lower bound; see
  // max_arrival_interval_error below.
  constexpr base::TimeDelta kMaxIntervalError = base::Milliseconds(3);
  // The test fails if the callbacks are not all received within this time.
  constexpr base::TimeDelta kCallbackTimeout = base::Seconds(5);

  if (!IsSupported()) {
    GTEST_SKIP() << "Not supported on this macOS version";
  }

  const CGDirectDisplayID display_id = CGMainDisplayID();
  if (display_id == kCGNullDirectDisplay) {
    GTEST_SKIP() << "No main display";
  }

  scoped_refptr<DisplayLinkMac> display_link =
      DisplayLinkMac::GetForDisplay(display_id);
  ASSERT_TRUE(display_link);
  const base::TimeDelta refresh_interval = display_link->GetRefreshInterval();
  EXPECT_TRUE(refresh_interval.is_positive());

  struct Sample {
    base::TimeTicks arrival_time;
    VSyncParamsMac params;
  };
  std::vector<Sample> samples;
  base::RunLoop run_loop;
  std::unique_ptr<VSyncCallbackMac> vsync_callback =
      display_link->RegisterCallback(
          base::BindLambdaForTesting([&](VSyncParamsMac params) {
            samples.push_back({base::TimeTicks::Now(), params});
            if (samples.size() >= kCallbacksToCollect) {
              run_loop.Quit();
            }
          }));
  ASSERT_TRUE(vsync_callback);

  {
    // If the callbacks do not all arrive in time, the run loop times out and
    // the test fails instead of hanging.
    base::test::ScopedRunLoopTimeout run_loop_timeout(FROM_HERE,
                                                      kCallbackTimeout);
    run_loop.Run();
  }
  ASSERT_GE(samples.size(), kCallbacksToCollect);

  // Diagnostics attached to every failure: measured arrival deltas alongside
  // the deltas of the timebases reported by the display link itself.
  std::string sample_log;
  for (size_t i = 1; i < samples.size(); ++i) {
    sample_log += base::StringPrintf(
        "\n  #%zu arrival_delta=%.3fms reported_timebase_delta=%.3fms "
        "reported_interval=%.3fms",
        i,
        (samples[i].arrival_time - samples[i - 1].arrival_time)
            .InMillisecondsF(),
        (samples[i].params.callback_timebase -
         samples[i - 1].params.callback_timebase)
            .InMillisecondsF(),
        samples[i].params.callback_interval.InMillisecondsF());
  }

  // Individual intervals jitter by a few milliseconds: arrivals with thread
  // scheduling, and CVDisplayLinkMac's reported timebases because they are
  // derived from the callback time rather than the hardware VSync. Averaging
  // over the measured callbacks cancels the jitter, while a wrong rate or a
  // stall still shows up, so all checks below use the average interval
  // (last time - first time) / (number of intervals) over
  // samples[kWarmUpCallbacks, kCallbacksToCollect).
  const Sample& first = samples[kWarmUpCallbacks];
  const Sample& last = samples[kCallbacksToCollect - 1];
  const size_t interval_count = kMeasuredCallbacks - 1;

  // The expected interval is the average of the intervals the display link
  // reported, falling back to the display refresh interval if the display
  // link did not report valid times.
  base::TimeDelta reported_interval_sum;
  size_t reported_interval_count = 0;
  for (size_t i = kWarmUpCallbacks; i < kCallbacksToCollect; ++i) {
    if (samples[i].params.callback_times_valid) {
      reported_interval_sum += samples[i].params.callback_interval;
      ++reported_interval_count;
    }
  }
  const base::TimeDelta expected_interval =
      reported_interval_count > 0
          ? reported_interval_sum / reported_interval_count
          : refresh_interval;

  // (1) Delivery to the registering thread. Arrival times include thread
  // hopping and scheduling delays on top of the display link's own timing, so
  // allow the larger of kMaxIntervalError and half the expected interval.
  const base::TimeDelta max_arrival_interval_error =
      std::max(kMaxIntervalError, expected_interval / 2);
  const base::TimeDelta average_arrival_interval =
      (last.arrival_time - first.arrival_time) / interval_count;
  EXPECT_LE((average_arrival_interval - expected_interval).magnitude(),
            max_arrival_interval_error)
      << "Average arrival interval " << average_arrival_interval
      << ", expected " << expected_interval << sample_log;

  // (2) The display link's own timing.
  if (first.params.callback_times_valid && last.params.callback_times_valid) {
    const base::TimeDelta average_timebase_interval =
        (last.params.callback_timebase - first.params.callback_timebase) /
        interval_count;
    EXPECT_LE((average_timebase_interval - expected_interval).magnitude(),
              kMaxIntervalError)
        << "Average reported timebase interval " << average_timebase_interval
        << ", expected " << expected_interval << sample_log;
  }

  // Destroying the VSyncCallbackMac unregisters it. Pump the run loop for a
  // few frames' worth of time and verify that no more callbacks arrive.
  vsync_callback.reset();
  const size_t sample_count_after_reset = samples.size();

  base::RunLoop idle_loop;
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, idle_loop.QuitClosure(), base::Milliseconds(300));
  idle_loop.Run();
  EXPECT_EQ(samples.size(), sample_count_after_reset);
}

INSTANTIATE_TEST_SUITE_P(All,
                         DisplayLinkMacTest,
                         testing::Values(DisplayLinkKind::kCVDisplayLink,
                                         DisplayLinkKind::kCADisplayLinkInGpu),
                         DisplayLinkKindToString);

}  // namespace
}  // namespace ui
