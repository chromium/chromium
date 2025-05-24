// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_METRICS_TRACKER_HOST_H_
#define UI_COMPOSITOR_COMPOSITOR_METRICS_TRACKER_HOST_H_

#include "base/functional/callback_forward.h"
#include "cc/metrics/frame_sequence_tracker.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

// An interface for CompositorMetricsTracker to call its host.
class COMPOSITOR_EXPORT CompositorMetricsTrackerHost {
 public:
  using TrackerId = size_t;

  virtual ~CompositorMetricsTrackerHost() = default;

  // Starts the tracking for the given `tracker_id`. `callback` is invoked after
  // the tracker is stopped and the metrics data is collected.
  using ReportCallback = base::OnceCallback<void(
      const cc::FrameSequenceMetrics::CustomReportData& data)>;
  virtual void StartMetricsTracker(TrackerId tracker_id,
                                   ReportCallback callback) = 0;

  // Stops the tracking for the given `tracker_id`. Returns true if tracking
  // is stopped successfully. Otherwise, returns false.
  virtual bool StopMetricsTracker(TrackerId tracker_id) = 0;

  // Cancels the tracking for the given tracker id.
  virtual void CancelMetricsTracker(TrackerId tracker_id) = 0;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_METRICS_TRACKER_HOST_H_
