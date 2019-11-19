// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_LATENCY_TRACKER_H_
#define UI_LATENCY_LATENCY_TRACKER_H_

#include "base/macros.h"
#include "ui/latency/average_lag_tracker.h"
#include "ui/latency/latency_info.h"

namespace ui {

// Utility class for tracking the latency of events. Relies on LatencyInfo
// components logged by content::RenderWidgetHostLatencyTracker.
class LatencyTracker {
 public:
  LatencyTracker();
  ~LatencyTracker();

  // Terminates latency tracking for events that triggered rendering, also
  // performing relevant UMA latency reporting.
  // Called when GPU buffers swap completes.
  void OnGpuSwapBuffersCompleted(const std::vector<LatencyInfo>& latency_info);
  void OnGpuSwapBuffersCompleted(const LatencyInfo& latency);

  using LatencyInfoProcessor =
      base::RepeatingCallback<void(const std::vector<ui::LatencyInfo>&)>;
  static void SetLatencyInfoProcessorForTesting(
      const LatencyInfoProcessor& processor);

 private:
  enum class InputMetricEvent {
    SCROLL_BEGIN_TOUCH = 0,
    SCROLL_UPDATE_TOUCH,
    SCROLL_BEGIN_WHEEL,
    SCROLL_UPDATE_WHEEL,

    INPUT_METRIC_EVENT_MAX = SCROLL_UPDATE_WHEEL
  };

  void ReportUkmScrollLatency(
      const InputMetricEvent& metric_event,
      base::TimeTicks start_timestamp,
      base::TimeTicks time_to_scroll_update_swap_begin_timestamp,
      base::TimeTicks time_to_handled_timestamp,
      bool is_main_thread,
      const ukm::SourceId ukm_source_id);

  void ComputeEndToEndLatencyHistograms(
      base::TimeTicks gpu_swap_begin_timestamp,
      base::TimeTicks gpu_swap_end_timestamp,
      const LatencyInfo& latency);

  AverageLagTracker average_lag_tracker_;

  DISALLOW_COPY_AND_ASSIGN(LatencyTracker);
};

}  // namespace latency

#endif  // UI_LATENCY_LATENCY_TRACKER_H_
