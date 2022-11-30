// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_COMPOSITOR_ANIMATION_RUNNER_H_
#define UI_VIEWS_ANIMATION_COMPOSITOR_ANIMATION_RUNNER_H_

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;

// An animation runner based on ui::Compositor.
class VIEWS_EXPORT CompositorAnimationRunner
    : public gfx::AnimationRunner,
      public ui::CompositorAnimationObserver,
      public WidgetObserver {
 public:
  explicit CompositorAnimationRunner(
      Widget* widget,
      const base::Location& location = FROM_HERE);
  CompositorAnimationRunner(CompositorAnimationRunner&) = delete;
  CompositorAnimationRunner& operator=(CompositorAnimationRunner&) = delete;
  ~CompositorAnimationRunner() override;

  // gfx::AnimationRunner:
  void Stop() override;

  // ui::CompositorAnimationObserver:
  void OnAnimationStep(base::TimeTicks timestamp) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  // WidgetObserver:
  void OnWidgetDestroying(Widget* widget) override;

 protected:
  // gfx::AnimationRunner:
  void OnStart(base::TimeDelta min_interval, base::TimeDelta elapsed) override;

 private:
  // Called when an animation is stopped, the compositor is shutting down, or
  // the widget is destroyed.
  void StopInternal();

  // When |widget_| is nullptr, it means the widget has been destroyed and
  // |compositor_| must also be nullptr.
  raw_ptr<Widget> widget_;

  // When |compositor_| is nullptr, it means either the animation is not
  // running, or the compositor or |widget_| associated with the compositor_ has
  // been destroyed during animation.
  raw_ptr<ui::Compositor> compositor_ = nullptr;

  base::TimeDelta min_interval_ = base::TimeDelta::Max();
  base::TimeTicks last_tick_;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_COMPOSITOR_ANIMATION_RUNNER_H_
