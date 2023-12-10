// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LOTTIE_ANIMATION_OBSERVER_H_
#define UI_LOTTIE_ANIMATION_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace lottie {
class Animation;

class COMPONENT_EXPORT(UI_LOTTIE) AnimationObserver
    : public base::CheckedObserver {
 public:
  // Called when the animation started playing.
  virtual void AnimationWillStartPlaying(const Animation* animation) {}

  // Called when one animation cycle has completed. This happens when a linear
  // animation has reached its end, or a loop/throbbing animation has finished
  // a cycle.
  virtual void AnimationCycleEnded(const Animation* animation) {}

  // Called when the animation has successfully resumed.
  virtual void AnimationResuming(const Animation* animation) {}

  // Called after each animation frame is painted. Note this is not synonymous
  // with the frame ultimately being rendered on screen; it only means the frame
  // has been submitted to the rest of the graphics pipeline for rendering.
  //
  // |t| is the normalized timestamp in range [0, 1] of the frame just painted.
  virtual void AnimationFramePainted(const Animation* animation, float t) {}

  // Called when the animation is `Stop()`ed.
  virtual void AnimationStopped(const Animation* animation) {}

  // Called in the Animation's destructor. Observers may remove themselves
  // within their implementation.
  virtual void AnimationIsDeleting(const Animation* animation) {}

 protected:
  ~AnimationObserver() override = default;
};

}  // namespace lottie

#endif  // UI_LOTTIE_ANIMATION_OBSERVER_H_
