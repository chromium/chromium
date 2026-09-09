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
    : ui::CompositorAnimationObserver(location) {
  widget_observation_.Observe(widget);
}

CompositorAnimationRunner::~CompositorAnimationRunner() {
  StopInternal();
  widget_observation_.Reset();
  CHECK(!IsInObserverList());
}

void CompositorAnimationRunner::Stop() {
  StopInternal();
}

void CompositorAnimationRunner::OnAnimationStep(base::TimeTicks timestamp) {
  if (timestamp < start_tick_) [[unlikely]] {
    return;
  }
  Step(timestamp);
}

void CompositorAnimationRunner::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  StopInternal();
}

void CompositorAnimationRunner::OnWidgetDestroying(Widget* widget) {
  StopInternal();
  widget_observation_.Reset();
}

void CompositorAnimationRunner::OnStart(base::TimeDelta min_interval,
                                        base::TimeDelta elapsed) {
  Widget* widget = widget_observation_.GetSource();
  if (!widget) {
    return;
  }

  // Reset the current compositor observation.
  StopInternal();

  ui::Compositor* compositor = widget->GetCompositor();
  if (!compositor) {
     return;
  }

  start_tick_ = base::TimeTicks::Now() - elapsed;
  compositor_observation_.Observe(compositor);
}

void CompositorAnimationRunner::StopInternal() {
  compositor_observation_.Reset();
}

}  // namespace views
