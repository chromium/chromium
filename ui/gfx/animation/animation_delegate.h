// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_ANIMATION_DELEGATE_H_
#define UI_GFX_ANIMATION_ANIMATION_DELEGATE_H_

#include "ui/gfx/animation/animation_export.h"

namespace gfx {

class Animation;
class AnimationContainer;

// AnimationDelegate
//
//  Implement this interface when you want to receive notifications about the
//  state of an animation.
class ANIMATION_EXPORT AnimationDelegate {
 public:
  virtual ~AnimationDelegate() {}

  // Called when an animation has completed.
  virtual void AnimationEnded(const Animation* animation) {}

  // Called when an animation has progressed.
  virtual void AnimationProgressed(const Animation* animation) {}

  // Called when an animation has been canceled.
  virtual void AnimationCanceled(const Animation* animation) {}

  // Called when an animation container has been set. This gives a chance to
  // set a custom animation runner.
  virtual void AnimationContainerWasSet(AnimationContainer* container) {}
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_ANIMATION_DELEGATE_H_
