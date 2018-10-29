// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SKIA_VECTOR_ANIMATION_OBSERVER_H_
#define UI_GFX_SKIA_VECTOR_ANIMATION_OBSERVER_H_

#include "ui/gfx/gfx_export.h"

namespace gfx {
class SkiaVectorAnimation;

class GFX_EXPORT SkiaVectorAnimationObserver {
 public:
  // Called when the animation started playing.
  virtual void AnimationWillStartPlaying(const SkiaVectorAnimation* animation) {
  }

  // Called when one animation cycle has completed. This happens when a linear
  // animation has reached its end, or a loop/throbbing animation has finished
  // a cycle.
  virtual void AnimationCycleEnded(const SkiaVectorAnimation* animation) {}

  // Called when the animation has successfully resumed.
  virtual void AnimationResuming(const SkiaVectorAnimation* animation) {}

 protected:
  virtual ~SkiaVectorAnimationObserver() = default;
};

}  // namespace gfx

#endif  // UI_GFX_SKIA_VECTOR_ANIMATION_OBSERVER_H_
