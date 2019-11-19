// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/flood_fill_ink_drop_ripple.h"

#include <algorithm>

#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/animation/ink_drop_util.h"
#include "ui/views/style/platform_style.h"

namespace {

// The minimum radius to use when scaling the painted layers. Smaller values
// were causing visual anomalies.
constexpr float kMinRadius = 1.f;

// All the sub animations that are used to animate each of the InkDropStates.
// These are used to get time durations with
// GetAnimationDuration(InkDropSubAnimations). Note that in general a sub
// animation defines the duration for either a transformation animation or an
// opacity animation but there are some exceptions where an entire InkDropState
// animation consists of only 1 sub animation and it defines the duration for
// both the transformation and opacity animations.
enum InkDropSubAnimations {
  // HIDDEN sub animations.

  // The HIDDEN sub animation that is fading out to a hidden opacity.
  HIDDEN_FADE_OUT,

  // The HIDDEN sub animation that transform the circle to a small one.
  HIDDEN_TRANSFORM,

  // ACTION_PENDING sub animations.

  // The ACTION_PENDING sub animation that fades in to the visible opacity.
  ACTION_PENDING_FADE_IN,

  // The ACTION_PENDING sub animation that transforms the circle to fill the
  // bounds.
  ACTION_PENDING_TRANSFORM,

  // ACTION_TRIGGERED sub animations.

  // The ACTION_TRIGGERED sub animation that is fading out to a hidden opacity.
  ACTION_TRIGGERED_FADE_OUT,

  // ALTERNATE_ACTION_PENDING sub animations.

  // The ALTERNATE_ACTION_PENDING animation has only one sub animation which
  // animates
  // the circleto fill the bounds at visible opacity.
  ALTERNATE_ACTION_PENDING,

  // ALTERNATE_ACTION_TRIGGERED sub animations.

  // The ALTERNATE_ACTION_TRIGGERED sub animation that is fading out to a hidden
  // opacity.
  ALTERNATE_ACTION_TRIGGERED_FADE_OUT,

  // ACTIVATED sub animations.

  // The ACTIVATED sub animation that is fading in to the visible opacity.
  ACTIVATED_FADE_IN,

  // The ACTIVATED sub animation that transforms the circle to fill the entire
  // bounds.
  ACTIVATED_TRANSFORM,

  // DEACTIVATED sub animations.

  // The DEACTIVATED sub animation that is fading out to a hidden opacity.
  DEACTIVATED_FADE_OUT,
};

// Duration constants for InkDropStateSubAnimations. See the
// InkDropStateSubAnimations enum documentation for more info.
int kAnimationDurationInMs[] = {
    200,  // HIDDEN_FADE_OUT
    300,  // HIDDEN_TRANSFORM
    0,    // ACTION_PENDING_FADE_IN
    240,  // ACTION_PENDING_TRANSFORM
    300,  // ACTION_TRIGGERED_FADE_OUT
    200,  // ALTERNATE_ACTION_PENDING
    300,  // ALTERNATE_ACTION_TRIGGERED_FADE_OUT
    150,  // ACTIVATED_FADE_IN
    200,  // ACTIVATED_TRANSFORM
    300,  // DEACTIVATED_FADE_OUT
};

gfx::Rect CalculateClipBounds(const gfx::Size& host_size,
                              const gfx::Insets& clip_insets) {
  gfx::Rect clip_bounds(host_size);
  clip_bounds.Inset(clip_insets);
  return clip_bounds;
}

float CalculateCircleLayerRadius(const gfx::Rect& clip_bounds) {
  return std::max(clip_bounds.width(), clip_bounds.height()) / 2.f;
}

}  // namespace

namespace views {

FloodFillInkDropRipple::FloodFillInkDropRipple(const gfx::Size& host_size,
                                               const gfx::Insets& clip_insets,
                                               const gfx::Point& center_point,
                                               SkColor color,
                                               float visible_opacity)
    : clip_insets_(clip_insets),
      center_point_(center_point),
      visible_opacity_(visible_opacity),
      use_hide_transform_duration_for_hide_fade_out_(false),
      duration_factor_(1.f),
      root_layer_(ui::LAYER_NOT_DRAWN),
      circle_layer_delegate_(color,
                             CalculateCircleLayerRadius(
                                 CalculateClipBounds(host_size, clip_insets))),
      ink_drop_state_(InkDropState::HIDDEN) {
  gfx::Rect clip_bounds = CalculateClipBounds(host_size, clip_insets);
  root_layer_.set_name("FloodFillInkDropRipple:ROOT_LAYER");
  root_layer_.SetMasksToBounds(true);
  root_layer_.SetBounds(clip_bounds);

  const int painted_size_length =
      std::max(clip_bounds.width(), clip_bounds.height());

  painted_layer_.SetBounds(gfx::Rect(painted_size_length, painted_size_length));
  painted_layer_.SetFillsBoundsOpaquely(false);
  painted_layer_.set_delegate(&circle_layer_delegate_);
  painted_layer_.SetVisible(true);
  painted_layer_.SetOpacity(1.0);
  painted_layer_.SetMasksToBounds(false);
  painted_layer_.set_name("FloodFillInkDropRipple:PAINTED_LAYER");

  root_layer_.Add(&painted_layer_);

  SetStateToHidden();
}

FloodFillInkDropRipple::FloodFillInkDropRipple(const gfx::Size& host_size,
                                               const gfx::Point& center_point,
                                               SkColor color,
                                               float visible_opacity)
    : FloodFillInkDropRipple(host_size,
                             gfx::Insets(),
                             center_point,
                             color,
                             visible_opacity) {}

FloodFillInkDropRipple::~FloodFillInkDropRipple() {
  // Explicitly aborting all the animations ensures all callbacks are invoked
  // while this instance still exists.
  AbortAllAnimations();
}

void FloodFillInkDropRipple::SnapToActivated() {
  InkDropRipple::SnapToActivated();
  SetOpacity(visible_opacity_);
  painted_layer_.SetTransform(GetMaxSizeTargetTransform());
}

ui::Layer* FloodFillInkDropRipple::GetRootLayer() {
  return &root_layer_;
}

void FloodFillInkDropRipple::AnimateStateChange(
    InkDropState old_ink_drop_state,
    InkDropState new_ink_drop_state,
    ui::LayerAnimationObserver* animation_observer) {
  switch (new_ink_drop_state) {
    case InkDropState::HIDDEN:
      if (!IsVisible()) {
        SetStateToHidden();
      } else {
        AnimateToOpacity(kHiddenOpacity, GetAnimationDuration(HIDDEN_FADE_OUT),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::EASE_IN_OUT, animation_observer);
        const gfx::Transform transform = CalculateTransform(kMinRadius);
        AnimateToTransform(transform, GetAnimationDuration(HIDDEN_TRANSFORM),
                           ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                           gfx::Tween::EASE_IN_OUT, animation_observer);
      }
      break;
    case InkDropState::ACTION_PENDING: {
      DLOG_IF(WARNING, InkDropState::HIDDEN != old_ink_drop_state)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      AnimateToOpacity(visible_opacity_,
                       GetAnimationDuration(ACTION_PENDING_FADE_IN),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      PauseOpacityAnimation(GetAnimationDuration(ACTION_PENDING_TRANSFORM) -
                                GetAnimationDuration(ACTION_PENDING_FADE_IN),
                            ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                            animation_observer);

      AnimateToTransform(GetMaxSizeTargetTransform(),
                         GetAnimationDuration(ACTION_PENDING_TRANSFORM),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::FAST_OUT_SLOW_IN, animation_observer);
      break;
    }
    case InkDropState::ACTION_TRIGGERED: {
      DLOG_IF(WARNING, old_ink_drop_state != InkDropState::HIDDEN &&
                           old_ink_drop_state != InkDropState::ACTION_PENDING)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      if (old_ink_drop_state == InkDropState::HIDDEN) {
        AnimateStateChange(old_ink_drop_state, InkDropState::ACTION_PENDING,
                           animation_observer);
      }
      AnimateToOpacity(kHiddenOpacity,
                       GetAnimationDuration(ACTION_TRIGGERED_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::ALTERNATE_ACTION_PENDING: {
      DLOG_IF(WARNING, InkDropState::ACTION_PENDING != old_ink_drop_state)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      AnimateToOpacity(visible_opacity_,
                       GetAnimationDuration(ALTERNATE_ACTION_PENDING),
                       ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                       gfx::Tween::EASE_IN, animation_observer);
      AnimateToTransform(GetMaxSizeTargetTransform(),
                         GetAnimationDuration(ALTERNATE_ACTION_PENDING),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
    }
    case InkDropState::ALTERNATE_ACTION_TRIGGERED:
      DLOG_IF(WARNING,
              InkDropState::ALTERNATE_ACTION_PENDING != old_ink_drop_state)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      AnimateToOpacity(
          kHiddenOpacity,
          GetAnimationDuration(ALTERNATE_ACTION_TRIGGERED_FADE_OUT),
          ui::LayerAnimator::ENQUEUE_NEW_ANIMATION, gfx::Tween::EASE_IN_OUT,
          animation_observer);
      break;
    case InkDropState::ACTIVATED: {
      if (old_ink_drop_state == InkDropState::ACTION_PENDING) {
        // The final state of pending animation is the same as the final state
        // of activated animation. We only need to enqueue a zero-length pause
        // so that animation observers are notified in order.
        PauseOpacityAnimation(
            base::TimeDelta(),
            ui::LayerAnimator::PreemptionStrategy::ENQUEUE_NEW_ANIMATION,
            animation_observer);
        PauseTransformAnimation(
            base::TimeDelta(),
            ui::LayerAnimator::PreemptionStrategy::ENQUEUE_NEW_ANIMATION,
            animation_observer);
      } else {
        AnimateToOpacity(visible_opacity_,
                         GetAnimationDuration(ACTIVATED_FADE_IN),
                         ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                         gfx::Tween::EASE_IN, animation_observer);
        AnimateToTransform(GetMaxSizeTargetTransform(),
                           GetAnimationDuration(ACTIVATED_TRANSFORM),
                           ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
                           gfx::Tween::EASE_IN_OUT, animation_observer);
      }
      break;
    }
    case InkDropState::DEACTIVATED:
      AnimateToOpacity(kHiddenOpacity,
                       GetAnimationDuration(DEACTIVATED_FADE_OUT),
                       ui::LayerAnimator::ENQUEUE_NEW_ANIMATION,
                       gfx::Tween::EASE_IN_OUT, animation_observer);
      break;
  }
}

void FloodFillInkDropRipple::SetStateToHidden() {
  painted_layer_.SetTransform(CalculateTransform(kMinRadius));
  root_layer_.SetOpacity(kHiddenOpacity);
  root_layer_.SetVisible(false);
}

void FloodFillInkDropRipple::AbortAllAnimations() {
  root_layer_.GetAnimator()->AbortAllAnimations();
  painted_layer_.GetAnimator()->AbortAllAnimations();
}

void FloodFillInkDropRipple::AnimateToTransform(
    const gfx::Transform& transform,
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    gfx::Tween::Type tween,
    ui::LayerAnimationObserver* animation_observer) {
  ui::LayerAnimator* animator = painted_layer_.GetAnimator();
  ui::ScopedLayerAnimationSettings animation(animator);
  animation.SetPreemptionStrategy(preemption_strategy);
  animation.SetTweenType(tween);

  std::unique_ptr<ui::LayerAnimationElement> element =
      ui::LayerAnimationElement::CreateTransformElement(transform, duration);

  ui::LayerAnimationSequence* sequence =
      new ui::LayerAnimationSequence(std::move(element));

  if (animation_observer)
    sequence->AddObserver(animation_observer);

  animator->StartAnimation(sequence);
}

void FloodFillInkDropRipple::PauseTransformAnimation(
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    ui::LayerAnimationObserver* observer) {
  ui::LayerAnimator* animator = painted_layer_.GetAnimator();
  ui::ScopedLayerAnimationSettings animation(animator);
  animation.SetPreemptionStrategy(preemption_strategy);

  std::unique_ptr<ui::LayerAnimationElement> element =
      ui::LayerAnimationElement::CreatePauseElement(
          ui::LayerAnimationElement::TRANSFORM, duration);

  ui::LayerAnimationSequence* sequence =
      new ui::LayerAnimationSequence(std::move(element));

  if (observer)
    sequence->AddObserver(observer);

  animator->StartAnimation(sequence);
}

void FloodFillInkDropRipple::SetOpacity(float opacity) {
  root_layer_.SetOpacity(opacity);
}

void FloodFillInkDropRipple::AnimateToOpacity(
    float opacity,
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    gfx::Tween::Type tween,
    ui::LayerAnimationObserver* animation_observer) {
  ui::LayerAnimator* animator = root_layer_.GetAnimator();
  ui::ScopedLayerAnimationSettings animation_settings(animator);
  animation_settings.SetPreemptionStrategy(preemption_strategy);
  animation_settings.SetTweenType(tween);
  std::unique_ptr<ui::LayerAnimationElement> animation_element =
      ui::LayerAnimationElement::CreateOpacityElement(opacity, duration);
  ui::LayerAnimationSequence* animation_sequence =
      new ui::LayerAnimationSequence(std::move(animation_element));

  if (animation_observer)
    animation_sequence->AddObserver(animation_observer);

  animator->StartAnimation(animation_sequence);
}

void FloodFillInkDropRipple::PauseOpacityAnimation(
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    ui::LayerAnimationObserver* observer) {
  ui::LayerAnimator* animator = root_layer_.GetAnimator();
  ui::ScopedLayerAnimationSettings animation(animator);
  animation.SetPreemptionStrategy(preemption_strategy);

  std::unique_ptr<ui::LayerAnimationElement> element =
      ui::LayerAnimationElement::CreatePauseElement(
          ui::LayerAnimationElement::OPACITY, duration);

  ui::LayerAnimationSequence* sequence =
      new ui::LayerAnimationSequence(std::move(element));

  if (observer)
    sequence->AddObserver(observer);

  animator->StartAnimation(sequence);
}

gfx::Transform FloodFillInkDropRipple::CalculateTransform(
    float target_radius) const {
  const float target_scale = target_radius / circle_layer_delegate_.radius();

  gfx::Transform transform = gfx::Transform();
  transform.Translate(center_point_.x() - root_layer_.bounds().x(),
                      center_point_.y() - root_layer_.bounds().y());
  transform.Scale(target_scale, target_scale);

  const gfx::Vector2dF drawn_center_offset =
      circle_layer_delegate_.GetCenteringOffset();
  transform.Translate(-drawn_center_offset.x(), -drawn_center_offset.y());

  // Add subpixel correction to the transform.
  transform.ConcatTransform(GetTransformSubpixelCorrection(
      transform, painted_layer_.device_scale_factor()));

  return transform;
}

gfx::Transform FloodFillInkDropRipple::GetMaxSizeTargetTransform() const {
  return CalculateTransform(MaxDistanceToCorners(center_point_));
}

float FloodFillInkDropRipple::MaxDistanceToCorners(
    const gfx::Point& point) const {
  const gfx::Rect bounds = root_layer_.bounds();
  const float distance_to_top_left = (bounds.origin() - point).Length();
  const float distance_to_top_right = (bounds.top_right() - point).Length();
  const float distance_to_bottom_left = (bounds.bottom_left() - point).Length();
  const float distance_to_bottom_right =
      (bounds.bottom_right() - point).Length();

  float largest_distance =
      std::max(distance_to_top_left, distance_to_top_right);
  largest_distance = std::max(largest_distance, distance_to_bottom_left);
  largest_distance = std::max(largest_distance, distance_to_bottom_right);
  return largest_distance;
}

// Returns the InkDropState sub animation duration for the given |state|.
base::TimeDelta FloodFillInkDropRipple::GetAnimationDuration(int state) {
  if (!PlatformStyle::kUseRipples ||
      !gfx::Animation::ShouldRenderRichAnimation()) {
    return base::TimeDelta();
  }

  int state_override = state;
  // Override the requested state if needed.
  if (use_hide_transform_duration_for_hide_fade_out_ &&
      state == HIDDEN_FADE_OUT) {
    state_override = HIDDEN_TRANSFORM;
  }

  return base::TimeDelta::FromMilliseconds(
      kAnimationDurationInMs[state_override] * duration_factor_);
}

}  // namespace views
