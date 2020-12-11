// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TOTAL_ANIMATION_THROUGHPUT_REPORTER_H_
#define UI_COMPOSITOR_TOTAL_ANIMATION_THROUGHPUT_REPORTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/throughput_tracker.h"

namespace ui {

// Reports cc::FrameSequenceMetrics::ThroughputData between the first animation
// start and the last animation ends on a compositor.
//
// Please see AnimationThroughputReporter for the definition of the throughput
// and jack metrics.
//
// See also docs/speed/graphics_metrics_definitions.md.
//
// cc::FrameSequenceMetrics::CustomReportData contains the numbers of produced
// frames, expected frames and jank count.
//
// The tracking starts when the first animation observer is added to the
// compositor, then stopped when the last animation observer is removed.  The
// report callback is invoked on the next begin frame if there is enough data.
// Since this observes multiple animations, aborting one of animations will
// not cancel the tracking, and the data will be reported as normal.
class COMPOSITOR_EXPORT TotalAnimationThroughputReporter
    : public CompositorObserver {
 public:
  using ReportOnceCallback = base::OnceCallback<void(
      const cc::FrameSequenceMetrics::CustomReportData& data)>;
  using ReportRepeatingCallback = base::RepeatingCallback<void(
      const cc::FrameSequenceMetrics::CustomReportData& data)>;

  // Create a TotalAnimationThroughputReporter that observes
  // the total animation throughput just once. If |should_delete|
  // is true, then the object will be deleted after callback is
  // invoked.
  TotalAnimationThroughputReporter(Compositor* compositor,
                                   ReportOnceCallback callback,
                                   bool should_delete);

  // Create a persistent TotalAnimationThroughputReporter, which
  // will call the callback every time the last animation is finished.
  TotalAnimationThroughputReporter(Compositor* compositor,
                                   ReportRepeatingCallback callback);

  TotalAnimationThroughputReporter(const TotalAnimationThroughputReporter&) =
      delete;
  TotalAnimationThroughputReporter& operator=(
      const TotalAnimationThroughputReporter&) = delete;
  ~TotalAnimationThroughputReporter() override;

  // CompositorObserver:
  void OnFirstAnimationStarted(Compositor* compositor) override;
  void OnLastAnimationEnded(Compositor* compositor) override;
  void OnCompositingShuttingDown(Compositor* compositor) override;

  bool IsMeasuringForTesting() const { return bool{throughput_tracker_}; }

 private:
  TotalAnimationThroughputReporter(Compositor* compositor,
                                   ReportRepeatingCallback repeating_callback,
                                   ReportOnceCallback once_callback,
                                   bool should_delete);

  void Report(const cc::FrameSequenceMetrics::CustomReportData& data);

  Compositor* compositor_;
  ReportRepeatingCallback report_repeating_callback_;
  ReportOnceCallback report_once_callback_;
  bool should_delete_ = false;
  base::Optional<ThroughputTracker> throughput_tracker_;

  base::WeakPtrFactory<TotalAnimationThroughputReporter> ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TOTAL_ANIMATION_THROUGHPUT_REPORTER_H_
