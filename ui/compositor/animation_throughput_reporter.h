// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_ANIMATION_THROUGHPUT_REPORTER_H_
#define UI_COMPOSITOR_ANIMATION_THROUGHPUT_REPORTER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "cc/metrics/frame_sequence_metrics.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

class Compositor;
class LayerAnimator;

// Reports cc::FrameSequenceMetrics::ThroughputData of layer animations.
//
// Throughput is defined as the ratio between number frames presented (actual
// screen updates) for the animations and the number frames expected.
// DroppedFrames/SkippedFrames is the other part of the story. It is a ratio
// between dropped/skipped frames over the expected frames.
//
// See also docs/speed/graphics_metrics_definitions.md.
//
// cc::FrameSequenceMetrics::ThroughputData contains the numbers of produced
// frames and expected frames and could be used to calculate the two metrics.
//
// All layer animation sequences created after its construction are consider
// as part of the animation being tracked. Graphics throughput tracking is
// stopped when all relevant layer animation sequences finish. The report
// callback is invoked on the next frame presentation if there is enough data
// and none of the layer animation sequences is aborted.
class COMPOSITOR_EXPORT AnimationThroughputReporter {
 public:
  using ReportCallback = base::RepeatingCallback<void(
      const cc::FrameSequenceMetrics::CustomReportData&)>;
  AnimationThroughputReporter(scoped_refptr<LayerAnimator> animator,
                              ReportCallback report_callback);
  AnimationThroughputReporter(const AnimationThroughputReporter&) = delete;
  AnimationThroughputReporter& operator=(const AnimationThroughputReporter&) =
      delete;
  ~AnimationThroughputReporter();

 private:
  // Tracks when layer animation sequences are scheduled and finished.
  class AnimationTracker;

  // Returns the relevant compositor of |animator|. Note it could return
  // nullptr if |animator| has not attached to an animation timeline.
  // Listed here to access LayerAnimator's protected delegate().
  static Compositor* GetCompositor(LayerAnimator* animator);

  // Whether |animator_| is attached to a timeline.
  // List here to access LayerAnimation's private |anmation_| member.
  static bool IsAnimatorAttachedToTimeline(LayerAnimator* animator);

  scoped_refptr<LayerAnimator> animator_;
  std::unique_ptr<AnimationTracker> animation_tracker_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_ANIMATION_THROUGHPUT_REPORTER_H_
