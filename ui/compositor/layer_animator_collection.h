// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_LAYER_ANIMATOR_COLLECTION_H_
#define UI_COMPOSITOR_LAYER_ANIMATOR_COLLECTION_H_

#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_export.h"

namespace ui {

class Compositor;
class LayerAnimator;

// A collection of LayerAnimators that should be updated at each animation step
// in the compositor.
class COMPOSITOR_EXPORT LayerAnimatorCollection
    : public CompositorAnimationObserver {
 public:
  explicit LayerAnimatorCollection(Compositor* compositor);
  ~LayerAnimatorCollection() override;

  void StartAnimator(scoped_refptr<LayerAnimator> animator);
  void StopAnimator(scoped_refptr<LayerAnimator> animator);

  bool HasActiveAnimators() const;

  base::TimeTicks last_tick_time() const { return last_tick_time_; }

  // CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(Compositor* compositor) override;

 private:
  Compositor* compositor_;
  base::TimeTicks last_tick_time_;
  std::set<scoped_refptr<LayerAnimator> > animators_;

  DISALLOW_COPY_AND_ASSIGN(LayerAnimatorCollection);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_LAYER_ANIMATOR_COLLECTION_H_
