// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/window_animations.h"

#include <math.h>

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"
#include "ui/base/class_property.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/core/wm_core_switches.h"
#include "ui/wm/public/animation_host.h"

namespace wm {
namespace {

// A base class for hiding animation observer which has two roles:
// 1) Notifies AnimationHost at the end of hiding animation.
// 2) Detaches the window's layers for hiding animation and deletes
// them upon completion of the animation. This is necessary to a)
// ensure that the animation continues in the event of the window being
// deleted, and b) to ensure that the animation is visible even if the
// window gets restacked below other windows when focus or activation
// changes.
// The subclass will determine when the animation is completed.
class HidingWindowAnimationObserverBase : public aura::WindowObserver {
 public:
  explicit HidingWindowAnimationObserverBase(aura::Window* window)
      : window_(window) {
    window_->AddObserver(this);
  }
  ~HidingWindowAnimationObserverBase() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK_EQ(window, window_);
    WindowInvalid();
  }

  void OnWindowDestroyed(aura::Window* window) override {
    DCHECK_EQ(window, window_);
    WindowInvalid();
  }

  // Detach the current layers and create new layers for |window_|.
  // Stack the original layers above |window_| and its transient
  // children.  If the window has transient children, the original
  // layers will be moved above the top most transient child so that
  // activation change does not put the window above the animating
  // layer.
  void DetachAndRecreateLayers() {
    layer_owner_ = RecreateLayers(window_);
    if (window_->parent()) {
      const aura::Window::Windows& transient_children =
          GetTransientChildren(window_);
      auto iter = std::find(window_->parent()->children().begin(),
                            window_->parent()->children().end(), window_);
      DCHECK(iter != window_->parent()->children().end());
      aura::Window* topmost_transient_child = NULL;
      for (++iter; iter != window_->parent()->children().end(); ++iter) {
        if (base::Contains(transient_children, *iter))
          topmost_transient_child = *iter;
      }
      if (topmost_transient_child) {
        window_->parent()->layer()->StackAbove(
            layer_owner_->root(), topmost_transient_child->layer());
      }
    }
    // Reset the transform for the |window_|. Because the animation may have
    // changed the transform, when recreating the layers we need to reset the
    // transform otherwise the recreated layer has the transform installed
    // for the animation.
    window_->layer()->SetTransform(gfx::Transform());
  }

 protected:
  // Invoked when the hiding animation is completed.  It will delete
  // 'this', and no operation should be made on this object after this
  // point.
  void OnAnimationCompleted() {
    // Window may have been destroyed by this point.
    if (window_) {
      AnimationHost* animation_host = GetAnimationHost(window_);
      if (animation_host)
        animation_host->OnWindowHidingAnimationCompleted();
      window_->RemoveObserver(this);
    }
    delete this;
  }

 private:
  // Invoked when the window is destroyed (or destroying).
  void WindowInvalid() {
    layer_owner_->root()->SuppressPaint();

    window_->RemoveObserver(this);
    window_ = NULL;
  }

  aura::Window* window_;

  // The owner of detached layers.
  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;

  DISALLOW_COPY_AND_ASSIGN(HidingWindowAnimationObserverBase);
};

class HidingWindowMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  HidingWindowMetricsReporter() = default;
  ~HidingWindowMetricsReporter() override = default;

  void Report(int value) override {
    UMA_HISTOGRAM_PERCENTAGE("Ash.Window.AnimationSmoothness.Hide", value);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(HidingWindowMetricsReporter);
};

base::LazyInstance<HidingWindowMetricsReporter>::Leaky g_reporter_hide =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// A HidingWindowAnimationObserver that deletes observer and detached
// layers upon the completion of the implicit animation.
class ImplicitHidingWindowAnimationObserver
    : public HidingWindowAnimationObserverBase,
      public ui::ImplicitAnimationObserver {
 public:
  ImplicitHidingWindowAnimationObserver(
      aura::Window* window,
      ui::ScopedLayerAnimationSettings* settings);
  ~ImplicitHidingWindowAnimationObserver() override {}

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImplicitHidingWindowAnimationObserver);
};

namespace {

const int kDefaultAnimationDurationForMenuMS = 150;

const float kWindowAnimation_HideOpacity = 0.f;
const float kWindowAnimation_ShowOpacity = 1.f;
const float kWindowAnimation_TranslateFactor = 0.5f;
const float kWindowAnimation_ScaleFactor = .95f;

const int kWindowAnimation_Rotate_DurationMS = 180;
const int kWindowAnimation_Rotate_OpacityDurationPercent = 90;
const float kWindowAnimation_Rotate_TranslateY = -20.f;
const float kWindowAnimation_Rotate_PerspectiveDepth = 500.f;
const float kWindowAnimation_Rotate_DegreesX = 5.f;
const float kWindowAnimation_Rotate_ScaleFactor = .99f;

const float kWindowAnimation_Bounce_Scale = 1.02f;
const int kWindowAnimation_Bounce_DurationMS = 180;
const int kWindowAnimation_Bounce_GrowShrinkDurationPercent = 40;

base::TimeDelta GetWindowVisibilityAnimationDuration(
    const aura::Window& window) {
  base::TimeDelta duration =
      window.GetProperty(kWindowVisibilityAnimationDurationKey);
  if (duration.is_zero() && window.type() == aura::client::WINDOW_TYPE_MENU) {
    return base::TimeDelta::FromMilliseconds(
        kDefaultAnimationDurationForMenuMS);
  }
  return duration;
}

// Gets/sets the WindowVisibilityAnimationType associated with a window.
// TODO(beng): redundant/fold into method on public api?
int GetWindowVisibilityAnimationType(aura::Window* window) {
  int type = window->GetProperty(kWindowVisibilityAnimationTypeKey);
  if (type == WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT) {
    return (window->type() == aura::client::WINDOW_TYPE_MENU ||
            window->type() == aura::client::WINDOW_TYPE_TOOLTIP)
               ? WINDOW_VISIBILITY_ANIMATION_TYPE_FADE
               : WINDOW_VISIBILITY_ANIMATION_TYPE_DROP;
  }
  return type;
}

void GetTransformRelativeToRoot(ui::Layer* layer, gfx::Transform* transform) {
  const ui::Layer* root = layer;
  while (root->parent())
    root = root->parent();
  layer->GetTargetTransformRelativeTo(root, transform);
}

gfx::Rect GetLayerWorldBoundsAfterTransform(ui::Layer* layer,
                                            const gfx::Transform& transform) {
  gfx::Transform in_world = transform;
  GetTransformRelativeToRoot(layer, &in_world);

  gfx::RectF transformed = gfx::RectF(layer->bounds());
  in_world.TransformRect(&transformed);

  return gfx::ToEnclosingRect(transformed);
}

// Augment the host window so that the enclosing bounds of the full
// animation will fit inside of it.
void AugmentWindowSize(aura::Window* window,
                       const gfx::Transform& end_transform) {
  AnimationHost* animation_host = GetAnimationHost(window);
  if (!animation_host)
    return;

  const gfx::Rect& world_at_start = window->bounds();
  gfx::Rect world_at_end =
      GetLayerWorldBoundsAfterTransform(window->layer(), end_transform);
  gfx::Rect union_in_window_space =
      gfx::UnionRects(world_at_start, world_at_end);

  // Calculate the top left and bottom right deltas to be added to the window
  // bounds.
  gfx::Vector2d top_left_delta(world_at_start.x() - union_in_window_space.x(),
                               world_at_start.y() - union_in_window_space.y());

  gfx::Vector2d bottom_right_delta(
      union_in_window_space.x() + union_in_window_space.width() -
          (world_at_start.x() + world_at_start.width()),
      union_in_window_space.y() + union_in_window_space.height() -
          (world_at_start.y() + world_at_start.height()));

  DCHECK(top_left_delta.x() >= 0 && top_left_delta.y() >= 0 &&
         bottom_right_delta.x() >= 0 && bottom_right_delta.y() >= 0);

  animation_host->SetHostTransitionOffsets(top_left_delta, bottom_right_delta);
}

// Shows a window using an animation, animating its opacity from 0.f to 1.f,
// its visibility to true, and its transform from |start_transform| to
// |end_transform|.
void AnimateShowWindowCommon(aura::Window* window,
                             const gfx::Transform& start_transform,
                             const gfx::Transform& end_transform) {
  AugmentWindowSize(window, end_transform);

  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetTransform(start_transform);
  window->layer()->SetVisible(true);

  {
    // Property sets within this scope will be implicitly animated.
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    base::TimeDelta duration = GetWindowVisibilityAnimationDuration(*window);
    if (duration > base::TimeDelta())
      settings.SetTransitionDuration(duration);

    window->layer()->SetTransform(end_transform);
    window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
  }
}

// Hides a window using an animation, animating its opacity from 1.f to 0.f,
// its visibility to false, and its transform to |end_transform|.
void AnimateHideWindowCommon(aura::Window* window,
                             const gfx::Transform& end_transform) {
  AugmentWindowSize(window, end_transform);

  // Property sets within this scope will be implicitly animated.
  ScopedHidingAnimationSettings hiding_settings(window);
  hiding_settings.layer_animation_settings()->SetAnimationMetricsReporter(
      g_reporter_hide.Pointer());
  // Render surface caching may not provide a benefit when animating the opacity
  // of a single layer.
  if (!window->layer()->children().empty())
    hiding_settings.layer_animation_settings()->CacheRenderSurface();
  base::TimeDelta duration = GetWindowVisibilityAnimationDuration(*window);
  if (duration > base::TimeDelta())
    hiding_settings.layer_animation_settings()->SetTransitionDuration(duration);

  window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
  window->layer()->SetTransform(end_transform);
  window->layer()->SetVisible(false);
}

static gfx::Transform GetScaleForWindow(aura::Window* window) {
  gfx::Rect bounds = window->bounds();
  gfx::Transform scale = gfx::GetScaleTransform(
      gfx::Point(kWindowAnimation_TranslateFactor * bounds.width(),
                 kWindowAnimation_TranslateFactor * bounds.height()),
      kWindowAnimation_ScaleFactor);
  return scale;
}

// Show/Hide windows using a shrink animation.
void AnimateShowWindow_Drop(aura::Window* window) {
  AnimateShowWindowCommon(window, GetScaleForWindow(window), gfx::Transform());
}

void AnimateHideWindow_Drop(aura::Window* window) {
  AnimateHideWindowCommon(window, GetScaleForWindow(window));
}

// Show/Hide windows using a vertical Glenimation.
void AnimateShowWindow_Vertical(aura::Window* window) {
  gfx::Transform transform;
  transform.Translate(0, window->GetProperty(
      kWindowVisibilityAnimationVerticalPositionKey));
  AnimateShowWindowCommon(window, transform, gfx::Transform());
}

void AnimateHideWindow_Vertical(aura::Window* window) {
  gfx::Transform transform;
  transform.Translate(0, window->GetProperty(
      kWindowVisibilityAnimationVerticalPositionKey));
  AnimateHideWindowCommon(window, transform);
}

// Show/Hide windows using a fade.
void AnimateShowWindow_Fade(aura::Window* window) {
  AnimateShowWindowCommon(window, gfx::Transform(), gfx::Transform());
}

void AnimateHideWindow_Fade(aura::Window* window) {
  AnimateHideWindowCommon(window, gfx::Transform());
}

std::unique_ptr<ui::LayerAnimationElement> CreateGrowShrinkElement(
    aura::Window* window,
    bool grow) {
  std::unique_ptr<ui::InterpolatedTransform> scale =
      std::make_unique<ui::InterpolatedScale>(
          gfx::Point3F(kWindowAnimation_Bounce_Scale,
                       kWindowAnimation_Bounce_Scale, 1),
          gfx::Point3F(1, 1, 1));
  std::unique_ptr<ui::InterpolatedTransform> scale_about_pivot =
      std::make_unique<ui::InterpolatedTransformAboutPivot>(
          gfx::Point(window->bounds().width() * 0.5,
                     window->bounds().height() * 0.5),
          std::move(scale));
  scale_about_pivot->SetReversed(grow);
  std::unique_ptr<ui::LayerAnimationElement> transition =
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(scale_about_pivot),
          base::TimeDelta::FromMilliseconds(
              kWindowAnimation_Bounce_DurationMS *
              kWindowAnimation_Bounce_GrowShrinkDurationPercent / 100));
  transition->set_tween_type(grow ? gfx::Tween::EASE_OUT : gfx::Tween::EASE_IN);
  return transition;
}

void AnimateBounce(aura::Window* window) {
  ui::ScopedLayerAnimationSettings scoped_settings(
      window->layer()->GetAnimator());
  scoped_settings.SetPreemptionStrategy(
      ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
  std::unique_ptr<ui::LayerAnimationSequence> sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  sequence->AddElement(CreateGrowShrinkElement(window, true));
  sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      ui::LayerAnimationElement::BOUNDS,
      base::TimeDelta::FromMilliseconds(
        kWindowAnimation_Bounce_DurationMS *
            (100 - 2 * kWindowAnimation_Bounce_GrowShrinkDurationPercent) /
            100)));
  sequence->AddElement(CreateGrowShrinkElement(window, false));
  window->layer()->GetAnimator()->StartAnimation(sequence.release());
}

// A HidingWindowAnimationObserver that deletes observer and detached
// layers when the last_sequence has been completed or aborted.
class RotateHidingWindowAnimationObserver
    : public HidingWindowAnimationObserverBase,
      public ui::LayerAnimationObserver {
 public:
  explicit RotateHidingWindowAnimationObserver(aura::Window* window)
      : HidingWindowAnimationObserverBase(window) {}
  ~RotateHidingWindowAnimationObserver() override {}

  // Destroys itself after |last_sequence| ends or is aborted. Does not take
  // ownership of |last_sequence|, which should not be NULL.
  void SetLastSequence(ui::LayerAnimationSequence* last_sequence) {
    last_sequence->AddObserver(this);
  }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    OnAnimationCompleted();
  }
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {
    OnAnimationCompleted();
  }
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RotateHidingWindowAnimationObserver);
};

void AddLayerAnimationsForRotate(aura::Window* window, bool show) {
  if (show)
    window->layer()->SetOpacity(kWindowAnimation_HideOpacity);

  base::TimeDelta duration = base::TimeDelta::FromMilliseconds(
      kWindowAnimation_Rotate_DurationMS);

  RotateHidingWindowAnimationObserver* observer = NULL;

  if (!show) {
    observer = new RotateHidingWindowAnimationObserver(window);
    window->layer()->GetAnimator()->SchedulePauseForProperties(
        duration * (100 - kWindowAnimation_Rotate_OpacityDurationPercent) / 100,
        ui::LayerAnimationElement::OPACITY);
  }
  std::unique_ptr<ui::LayerAnimationElement> opacity =
      ui::LayerAnimationElement::CreateOpacityElement(
          show ? kWindowAnimation_ShowOpacity : kWindowAnimation_HideOpacity,
          duration * kWindowAnimation_Rotate_OpacityDurationPercent / 100);
  opacity->set_tween_type(gfx::Tween::EASE_IN_OUT);
  window->layer()->GetAnimator()->ScheduleAnimation(
      new ui::LayerAnimationSequence(std::move(opacity)));

  float xcenter = window->bounds().width() * 0.5;

  gfx::Transform transform;
  transform.Translate(xcenter, 0);
  transform.ApplyPerspectiveDepth(kWindowAnimation_Rotate_PerspectiveDepth);
  transform.Translate(-xcenter, 0);
  std::unique_ptr<ui::InterpolatedTransform> perspective =
      std::make_unique<ui::InterpolatedConstantTransform>(transform);

  std::unique_ptr<ui::InterpolatedTransform> scale =
      std::make_unique<ui::InterpolatedScale>(
          1, kWindowAnimation_Rotate_ScaleFactor);
  std::unique_ptr<ui::InterpolatedTransform> scale_about_pivot =
      std::make_unique<ui::InterpolatedTransformAboutPivot>(
          gfx::Point(xcenter, kWindowAnimation_Rotate_TranslateY),
          std::move(scale));

  std::unique_ptr<ui::InterpolatedTransform> translation =
      std::make_unique<ui::InterpolatedTranslation>(
          gfx::PointF(), gfx::PointF(0, kWindowAnimation_Rotate_TranslateY));

  std::unique_ptr<ui::InterpolatedTransform> rotation =
      std::make_unique<ui::InterpolatedAxisAngleRotation>(
          gfx::Vector3dF(1, 0, 0), 0, kWindowAnimation_Rotate_DegreesX);

  scale_about_pivot->SetChild(std::move(perspective));
  translation->SetChild(std::move(scale_about_pivot));
  rotation->SetChild(std::move(translation));
  rotation->SetReversed(show);

  std::unique_ptr<ui::LayerAnimationElement> transition =
      ui::LayerAnimationElement::CreateInterpolatedTransformElement(
          std::move(rotation), duration);
  ui::LayerAnimationSequence* last_sequence =
      new ui::LayerAnimationSequence(std::move(transition));
  window->layer()->GetAnimator()->ScheduleAnimation(last_sequence);

  if (observer) {
    observer->SetLastSequence(last_sequence);
    observer->DetachAndRecreateLayers();
  }

  window->layer()->SetVisible(show);
}

void AnimateShowWindow_Rotate(aura::Window* window) {
  AddLayerAnimationsForRotate(window, true);
}

void AnimateHideWindow_Rotate(aura::Window* window) {
  AddLayerAnimationsForRotate(window, false);
}

bool AnimateShowWindow(aura::Window* window) {
  if (!HasWindowVisibilityAnimationTransition(window, ANIMATE_SHOW)) {
    if (HasWindowVisibilityAnimationTransition(window, ANIMATE_HIDE)) {
      // Since hide animation may have changed opacity and transform,
      // reset them to show the window.
      window->layer()->SetOpacity(kWindowAnimation_ShowOpacity);
      window->layer()->SetTransform(gfx::Transform());
    }
    return false;
  }

  switch (GetWindowVisibilityAnimationType(window)) {
    case WINDOW_VISIBILITY_ANIMATION_TYPE_DROP:
      AnimateShowWindow_Drop(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL:
      AnimateShowWindow_Vertical(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_FADE:
      AnimateShowWindow_Fade(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_ROTATE:
      AnimateShowWindow_Rotate(window);
      return true;
    default:
      return false;
  }
}

bool AnimateHideWindow(aura::Window* window) {
  if (!HasWindowVisibilityAnimationTransition(window, ANIMATE_HIDE)) {
    if (HasWindowVisibilityAnimationTransition(window, ANIMATE_SHOW)) {
      // Since show animation may have changed opacity and transform,
      // reset them, though the change should be hidden.
      window->layer()->SetOpacity(kWindowAnimation_HideOpacity);
      window->layer()->SetTransform(gfx::Transform());
    }
    return false;
  }

  switch (GetWindowVisibilityAnimationType(window)) {
    case WINDOW_VISIBILITY_ANIMATION_TYPE_DROP:
      AnimateHideWindow_Drop(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL:
      AnimateHideWindow_Vertical(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_FADE:
      AnimateHideWindow_Fade(window);
      return true;
    case WINDOW_VISIBILITY_ANIMATION_TYPE_ROTATE:
      AnimateHideWindow_Rotate(window);
      return true;
    default:
      return false;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ImplicitHidingWindowAnimationObserver

ImplicitHidingWindowAnimationObserver::ImplicitHidingWindowAnimationObserver(
    aura::Window* window,
    ui::ScopedLayerAnimationSettings* settings)
    : HidingWindowAnimationObserverBase(window) {
  settings->AddObserver(this);
}

void ImplicitHidingWindowAnimationObserver::OnImplicitAnimationsCompleted() {
  OnAnimationCompleted();
}

////////////////////////////////////////////////////////////////////////////////
// ScopedHidingAnimationSettings

ScopedHidingAnimationSettings::ScopedHidingAnimationSettings(
    aura::Window* window)
    : layer_animation_settings_(window->layer()->GetAnimator()),
      observer_(new ImplicitHidingWindowAnimationObserver(
          window,
          &layer_animation_settings_)) {
}

ScopedHidingAnimationSettings::~ScopedHidingAnimationSettings() {
  observer_->DetachAndRecreateLayers();
}

////////////////////////////////////////////////////////////////////////////////
// External interface

void SetWindowVisibilityAnimationType(aura::Window* window, int type) {
  window->SetProperty(kWindowVisibilityAnimationTypeKey, type);
}

int GetWindowVisibilityAnimationType(aura::Window* window) {
  return window->GetProperty(kWindowVisibilityAnimationTypeKey);
}

void SetWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition) {
  window->SetProperty(kWindowVisibilityAnimationTransitionKey, transition);
}

bool HasWindowVisibilityAnimationTransition(
    aura::Window* window,
    WindowVisibilityAnimationTransition transition) {
  WindowVisibilityAnimationTransition prop = window->GetProperty(
      kWindowVisibilityAnimationTransitionKey);
  return (prop & transition) != 0;
}

void SetWindowVisibilityAnimationDuration(aura::Window* window,
                                          const base::TimeDelta& duration) {
  window->SetProperty(kWindowVisibilityAnimationDurationKey, duration);
}

base::TimeDelta GetWindowVisibilityAnimationDuration(
    const aura::Window& window) {
  return window.GetProperty(kWindowVisibilityAnimationDurationKey);
}

void SetWindowVisibilityAnimationVerticalPosition(aura::Window* window,
                                                  float position) {
  window->SetProperty(kWindowVisibilityAnimationVerticalPositionKey, position);
}

bool AnimateOnChildWindowVisibilityChanged(aura::Window* window, bool visible) {
  if (WindowAnimationsDisabled(window))
    return false;
  if (visible)
    return AnimateShowWindow(window);
  // Don't start hiding the window again if it's already being hidden.
  return window->layer()->GetTargetOpacity() != 0.0f &&
      AnimateHideWindow(window);
}

bool AnimateWindow(aura::Window* window, WindowAnimationType type) {
  switch (type) {
  case WINDOW_ANIMATION_TYPE_BOUNCE:
    AnimateBounce(window);
    return true;
  default:
    NOTREACHED();
    return false;
  }
}

bool WindowAnimationsDisabled(aura::Window* window) {
  // Individual windows can choose to skip animations.
  if (window && window->GetProperty(aura::client::kAnimationsDisabledKey))
    return true;

  // Animations can be disabled globally for testing.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWindowAnimationsDisabled))
    return true;

  // Tests of animations themselves should still run even if the machine is
  // being accessed via Remote Desktop.
  if (ui::ScopedAnimationDurationScaleMode::duration_scale_mode() ==
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION)
    return false;

  // Let the user decide whether or not to play the animation.
  return !gfx::Animation::ShouldRenderRichAnimation();
}

}  // namespace wm
