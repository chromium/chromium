// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_ANIMATION_CONTAINER_OBSERVER_H_
#define UI_GFX_ANIMATION_ANIMATION_CONTAINER_OBSERVER_H_

#include "ui/gfx/animation/animation_export.h"

namespace gfx {

class AnimationContainer;

// The observer is notified after every update of the animations managed by
// the container.
class ANIMATION_EXPORT AnimationContainerObserver {
 public:
  // Invoked on every tick of the timer managed by the container and after
  // all the animations have updated.
  virtual void AnimationContainerProgressed(AnimationContainer* container) {}

  // Invoked when no more animations are being managed by this container.
  virtual void AnimationContainerEmpty(AnimationContainer* container) {}

  // Invoked from AnimationContainer's destructor.
  virtual void AnimationContainerShuttingDown(AnimationContainer* container) {}

 protected:
  virtual ~AnimationContainerObserver() {}
};

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_ANIMATION_CONTAINER_OBSERVER_H_
