// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TOTAL_ANIMATION_THROUGHPUT_REPORTER_H_
#define UI_COMPOSITOR_TOTAL_ANIMATION_THROUGHPUT_REPORTER_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/throughput_tracker.h"

namespace ash {
class LoginUnlockThroughputRecorderTestBase;
}

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
//
// The reporter will not fire if ScopedThroughputReporterBlocker is active.
// This allows to measure throughput from the very first animation (when
// reporter was created) till the specific expected animation ends even if
// there were delays between the animations.
class COMPOSITOR_EXPORT TotalAnimationThroughputReporter
    : public CompositorObserver {
 public:
  // This allows to temporarily ignore OnFirstNonAnimatedFrameStarted event
  // until an interesting event happens.
  class COMPOSITOR_EXPORT ScopedThroughputReporterBlocker {
   public:
    explicit ScopedThroughputReporterBlocker(
        base::WeakPtr<TotalAnimationThroughputReporter> reporter);
    ScopedThroughputReporterBlocker(const ScopedThroughputReporterBlocker&) =
        delete;
    ~ScopedThroughputReporterBlocker();

    ScopedThroughputReporterBlocker& operator=(
        const ScopedThroughputReporterBlocker&) = delete;

   private:
    base::WeakPtr<TotalAnimationThroughputReporter> reporter_;
  };

  using ReportOnceCallback = base::OnceCallback<void(
      const cc::FrameSequenceMetrics::CustomReportData& data,
      base::TimeTicks first_animation_started_at,
      base::TimeTicks last_animation_finished_at)>;
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
  void OnFirstNonAnimatedFrameStarted(Compositor* compositor) override;
  void OnCompositingShuttingDown(Compositor* compositor) override;

  base::WeakPtr<ui::TotalAnimationThroughputReporter> GetWeakPtr();

  bool IsMeasuringForTesting() const { return bool{throughput_tracker_}; }

  // The returned scope will delay the animation report until the next
  // |OnFirstNonAnimatedFrameStarted| received after it is destructed. See
  // |ScopedThroughputReporterBlocker| above.
  std::unique_ptr<ScopedThroughputReporterBlocker> NewScopedBlocker();

 private:
  friend class ash::LoginUnlockThroughputRecorderTestBase;

  TotalAnimationThroughputReporter(Compositor* compositor,
                                   ReportRepeatingCallback repeating_callback,
                                   ReportOnceCallback once_callback,
                                   bool should_delete);

  void Report(const cc::FrameSequenceMetrics::CustomReportData& data);

  // Returns true if there is an active ScopedThroughputReporterBlocker.
  bool IsBlocked() const;

  raw_ptr<Compositor> compositor_;
  ReportRepeatingCallback report_repeating_callback_;
  ReportOnceCallback report_once_callback_;
  bool should_delete_ = false;
  std::optional<ThroughputTracker> throughput_tracker_;

  // These are always recorderd in pairs. Specifically,
  // `timestamp_first_animation_started_at_` is recorded when
  // `throughput_tracker_` is created/started, and
  // `timestamp_last_animation_finished_at_` is recorded when the tracker is
  // stopped/destructed.
  base::TimeTicks timestamp_first_animation_started_at_;
  base::TimeTicks timestamp_last_animation_finished_at_;

  // Number of active ScopedThroughputReporterBlocker objects.
  int scoped_blocker_count_ = 0;

  base::WeakPtrFactory<TotalAnimationThroughputReporter> ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TOTAL_ANIMATION_THROUGHPUT_REPORTER_H_
