// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_ANIMATION_OBSERVER_H_
#define UI_COMPOSITOR_COMPOSITOR_ANIMATION_OBSERVER_H_

#include "base/time/time.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

class Compositor;

class COMPOSITOR_EXPORT CompositorAnimationObserver {
 public:
  virtual ~CompositorAnimationObserver() {}

  virtual void OnAnimationStep(base::TimeTicks timestamp) = 0;
  virtual void OnCompositingShuttingDown(Compositor* compositor) = 0;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_ANIMATION_OBSERVER_H_
