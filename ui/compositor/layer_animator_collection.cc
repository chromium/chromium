// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_animator_collection.h"

#include <set>

#include "base/time/time.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer_animator.h"

namespace ui {

LayerAnimatorCollection::LayerAnimatorCollection(Compositor* compositor)
    : compositor_(compositor), last_tick_time_(base::TimeTicks::Now()) {
  // Do not check the active duration for the LayerAnimationCollection because
  // new animation can be added while running existing animation, which
  // extends the duration.
  set_check_active_duration(false);
}

LayerAnimatorCollection::~LayerAnimatorCollection() {
  if (compositor_)
    compositor_->RemoveAnimationObserver(this);
}

void LayerAnimatorCollection::StartAnimator(
    scoped_refptr<LayerAnimator> animator) {
  DCHECK_EQ(0U, animators_.count(animator));
  if (animators_.empty())
    last_tick_time_ = base::TimeTicks::Now();
  animators_.insert(animator);
  if (animators_.size() == 1U && compositor_)
    compositor_->AddAnimationObserver(this);
}

void LayerAnimatorCollection::StopAnimator(
    scoped_refptr<LayerAnimator> animator) {
  DCHECK_GT(animators_.count(animator), 0U);
  animators_.erase(animator);
  if (animators_.empty() && compositor_)
    compositor_->RemoveAnimationObserver(this);
}

bool LayerAnimatorCollection::HasActiveAnimators() const {
  return !animators_.empty();
}

void LayerAnimatorCollection::OnAnimationStep(base::TimeTicks now) {
  last_tick_time_ = now;
  std::set<scoped_refptr<LayerAnimator> > list = animators_;
  for (auto iter = list.begin(); iter != list.end(); ++iter) {
    // Make sure the animator is still valid.
    if (animators_.count(*iter) > 0)
      (*iter)->Step(now);
  }
  if (!HasActiveAnimators() && compositor_)
    compositor_->RemoveAnimationObserver(this);
}

void LayerAnimatorCollection::OnCompositingShuttingDown(
    Compositor* compositor) {
  DCHECK_EQ(compositor_, compositor);
  compositor_->RemoveAnimationObserver(this);
  compositor_ = nullptr;
}

}  // namespace ui
