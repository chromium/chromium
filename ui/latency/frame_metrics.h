// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_FRAME_METRICS_H_
#define UI_LATENCY_FRAME_METRICS_H_

#include "ui/latency/stream_analyzer.h"

#include <cstdint>

#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "ui/latency/skipped_frame_tracker.h"

namespace ui {
namespace frame_metrics {

class SkipClient : public frame_metrics::StreamAnalyzerClient {
  double TransformResult(double result) const override;
};

class LatencyClient : public frame_metrics::StreamAnalyzerClient {
  double TransformResult(double result) const override;
};

class LatencySpeedClient : public frame_metrics::StreamAnalyzerClient {
  double TransformResult(double result) const override;
};

class LatencyAccelerationClient : public frame_metrics::StreamAnalyzerClient {
  double TransformResult(double result) const override;
};

}  // namespace frame_metrics

enum class FrameMetricsSource {
  Unknown = 0,
  UnitTest = 1,
  RendererCompositor = 2,
  UiCompositor = 3,
};

enum class FrameMetricsSourceThread {
  Unknown = 0,
  Blink = 1,
  RendererCompositor = 2,
  Ui = 3,
  UiCompositor = 4,
  VizCompositor = 5,
};

enum class FrameMetricsCompileTarget {
  Unknown = 0,
  Chromium = 1,
  SynchronousCompositor = 2,
  Headless = 3,
};

struct FrameMetricsSettings {
  FrameMetricsSettings() = default;

  FrameMetricsSettings(FrameMetricsSource source,
                       FrameMetricsSourceThread source_thread,
                       FrameMetricsCompileTarget compile_target,
                       bool trace_results_every_frame = false,
                       size_t max_window_size = 60)
      : source(source),
        source_thread(source_thread),
        compile_target(compile_target),
        trace_results_every_frame(trace_results_every_frame),
        max_window_size(max_window_size) {}

  void set_is_frame_latency_speed_on(bool is_speed_on) {
    is_frame_latency_speed_on_ = is_speed_on;
  }
  void set_is_frame_latency_acceleration_on(bool is_acceleration_on) {
    is_frame_latency_acceleration_on_ = is_acceleration_on;
  }

  bool is_frame_latency_speed_on() const { return is_frame_latency_speed_on_; }
  bool is_frame_latency_acceleration_on() const {
    return is_frame_latency_acceleration_on_;
  }

  // Source configuration.
  FrameMetricsSource source;
  FrameMetricsSourceThread source_thread;
  FrameMetricsCompileTarget compile_target;

  // This is needed for telemetry results.
  bool trace_results_every_frame;

  // Maximum window size in number of samples.
  // This is forwarded to each WindowAnalyzer.
  size_t max_window_size;

  void AsValueInto(base::trace_event::TracedValue* state) const;

 private:
  // Switch for frame latency speed measurements control.
  bool is_frame_latency_speed_on_ = false;

  // Switch for frame latency acceleration measurements control.
  bool is_frame_latency_acceleration_on_ = false;
};

// Calculates all metrics for a frame source.
// Every frame source that we wish to instrument will own an instance of
// this class and will call AddFrameProduced and AddFrameDisplayed.
// Statistics will be reported automatically. Either periodically, based
// on the client interface, or on destruction if any samples were added since
// the last call to StartNewReportPeriod.
class FrameMetrics : public SkippedFrameTracker::Client {
 public:
  explicit FrameMetrics(FrameMetricsSettings settings);
  ~FrameMetrics() override;

  // Resets all data and history as if the class were just created.
  void Reset();

  // AddFrameProduced should be called every time a source produces a frame.
  // |source_timestamp| is when frame time in BeginFrameArgs(i.e. when the frame
  // is produced); |amount_produced| is the expected time interval between 2
  // consecutive frames; |amount_skipped| is number of frame skipped before
  // producing this frame multiplies by the interval, i.e., if 1 frame is
  // skipped in 30 fps setting, then |amount_skipped| is 33.33ms; if 1 frame is
  // skipped in 60FPS setting, then the |amount_skipped| is 16.67ms. Note: If
  // the FrameMetrics class is hooked up to an optional SkippedFrameTracker, the
  // client should not call this directly.
  void AddFrameProduced(base::TimeTicks source_timestamp,
                        base::TimeDelta amount_produced,
                        base::TimeDelta amount_skipped) override;

  // AddFrameDisplayed should be called whenever a frame causes damage and
  // we know when the result became visible on the display. |source_timestamp|
  // is when frame time in BeginFrameArgs(i.e. when the frame is produced);
  // |display_timestamp| is when the frame is displayed on screen.
  // This will affect all latency derived metrics, including latency speed,
  // latency acceleration, and latency itself.
  // If a frame is produced but not displayed, do not call this; there was
  // no change in the displayed result and thus no change to track the visual
  // latency of. Guessing a displayed time will only skew the results.
  void AddFrameDisplayed(base::TimeTicks source_timestamp,
                         base::TimeTicks display_timestamp);

  // Compute the square root by using method described in paper:
  // http://www.lomont.org/Math/Papers/2003/InvSqrt.pdf.
  // It finds a result within 0.0001 and 0.1 of the true square root for |x| <
  // 100 and |x| < 2^15 respectively. It's more than 2 times faster for Nexus 4
  // and other lower end android devices and ~3-5% faster on desktop. Crash when
  // x is less than 0.
  static double FastApproximateSqrt(double x);

 protected:
  void TraceStats() const;

  // virtual for testing.
  virtual base::TimeDelta ReportPeriod();

  // Starts a new reporting period after |kDefaultReportPeriod| time that resets
  // the various accumulators and memory of worst regions encountered, but does
  // not destroy recent sample history in the windowed analyzers and in the
  // derivatives for latency speed and latency acceleration. This avoids small
  // gaps in coverage when starting a new reporting period.
  void StartNewReportPeriod();

  FrameMetricsSettings settings_;
  const char* source_name_;

  frame_metrics::SharedWindowedAnalyzerClient shared_skip_client_;
  base::circular_deque<base::TimeTicks> skip_timestamp_queue_;

  frame_metrics::SharedWindowedAnalyzerClient shared_latency_client_;
  base::circular_deque<base::TimeTicks> latency_timestamp_queue_;

  base::TimeDelta time_since_start_of_report_period_;
  uint32_t frames_produced_since_start_of_report_period_ = 0;

  uint64_t latencies_added_ = 0;
  base::TimeTicks source_timestamp_prev_;
  base::TimeDelta latency_prev_;
  base::TimeDelta source_duration_prev_;
  base::TimeDelta latency_delta_prev_;

  frame_metrics::SkipClient skip_client_;
  frame_metrics::LatencyClient latency_client_;
  frame_metrics::LatencySpeedClient latency_speed_client_;
  frame_metrics::LatencyAccelerationClient latency_acceleration_client_;

  frame_metrics::StreamAnalyzer frame_skips_analyzer_;
  frame_metrics::StreamAnalyzer latency_analyzer_;
  frame_metrics::StreamAnalyzer latency_speed_analyzer_;
  frame_metrics::StreamAnalyzer latency_acceleration_analyzer_;

  DISALLOW_COPY_AND_ASSIGN(FrameMetrics);
};

}  // namespace ui

#endif  // UI_LATENCY_FRAME_METRICS_H_
