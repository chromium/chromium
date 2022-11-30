// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/compositor_animation_runner.h"

#include "ui/views/widget/widget.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// CompositorAnimationRunner
//

CompositorAnimationRunner::CompositorAnimationRunner(
    Widget* widget,
    const base::Location& location)
    : ui::CompositorAnimationObserver(location), widget_(widget) {
  widget_->AddObserver(this);
}

CompositorAnimationRunner::~CompositorAnimationRunner() {
  // Make sure we're not observing |compositor_|.
  if (widget_)
    OnWidgetDestroying(widget_);
  DCHECK(!compositor_ || !compositor_->HasAnimationObserver(this));
  CHECK(!IsInObserverList());
}

void CompositorAnimationRunner::Stop() {
  StopInternal();
}

void CompositorAnimationRunner::OnAnimationStep(base::TimeTicks timestamp) {
  if (timestamp - last_tick_ < min_interval_)
    return;

  last_tick_ = timestamp;
  Step(last_tick_);
}

void CompositorAnimationRunner::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  StopInternal();
}

void CompositorAnimationRunner::OnWidgetDestroying(Widget* widget) {
  StopInternal();
  widget_->RemoveObserver(this);
  widget_ = nullptr;
}

void CompositorAnimationRunner::OnStart(base::TimeDelta min_interval,
                                        base::TimeDelta elapsed) {
  if (!widget_)
    return;

  ui::Compositor* current_compositor = widget_->GetCompositor();
  if (!current_compositor) {
    StopInternal();
    return;
  }

  if (current_compositor != compositor_) {
    if (compositor_ && compositor_->HasAnimationObserver(this))
      compositor_->RemoveAnimationObserver(this);
    compositor_ = current_compositor;
  }

  last_tick_ = base::TimeTicks::Now() - elapsed;
  min_interval_ = min_interval;
  DCHECK(!compositor_->HasAnimationObserver(this));
  compositor_->AddAnimationObserver(this);
}

void CompositorAnimationRunner::StopInternal() {
  if (compositor_ && compositor_->HasAnimationObserver(this))
    compositor_->RemoveAnimationObserver(this);

  min_interval_ = base::TimeDelta::Max();
  compositor_ = nullptr;
}

}  // namespace views
