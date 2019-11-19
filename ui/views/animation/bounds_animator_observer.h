// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_BOUNDS_ANIMATOR_OBSERVER_H_
#define UI_VIEWS_ANIMATION_BOUNDS_ANIMATOR_OBSERVER_H_

#include "ui/views/views_export.h"

namespace views {

class BoundsAnimator;

class VIEWS_EXPORT BoundsAnimatorObserver {
 public:
  // Invoked when animations have progressed.
  virtual void OnBoundsAnimatorProgressed(BoundsAnimator* animator) = 0;

  // Invoked when all animations are complete.
  virtual void OnBoundsAnimatorDone(BoundsAnimator* animator) = 0;

 protected:
  virtual ~BoundsAnimatorObserver() = default;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_BOUNDS_ANIMATOR_OBSERVER_H_
