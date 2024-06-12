// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/flood_fill_ink_drop_ripple.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop_util.h"
#include "ui/views/style/platform_style.h"

namespace {

// The minimum radius to use when scaling the painted layers. Smaller values
// were causing visual anomalies.
constexpr float kMinRadius = 1.f;

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

FloodFillInkDropRipple::FloodFillInkDropRipple(InkDropHost* ink_drop_host,
                                               const gfx::Size& host_size,
                                               const gfx::Insets& clip_insets,
                                               const gfx::Point& center_point,
                                               SkColor color,
                                               float visible_opacity)
    : InkDropRipple(ink_drop_host),
      clip_insets_(clip_insets),
      center_point_(center_point),
      visible_opacity_(visible_opacity),
      use_hide_transform_duration_for_hide_fade_out_(false),
      duration_factor_(1.f),
      root_layer_(ui::LAYER_NOT_DRAWN),
      circle_layer_delegate_(color,
                             CalculateCircleLayerRadius(
                                 CalculateClipBounds(host_size, clip_insets))) {
  gfx::Rect clip_bounds = CalculateClipBounds(host_size, clip_insets);
  root_layer_.SetName("FloodFillInkDropRipple:ROOT_LAYER");
  root_layer_.SetMasksToBounds(true);
  root_layer_.SetBounds(clip_bounds);
  root_callback_subscription_ =
      root_layer_.GetAnimator()->AddSequenceScheduledCallback(
          base::BindRepeating(
              &FloodFillInkDropRipple::OnLayerAnimationSequenceScheduled,
              base::Unretained(this)));

  const int painted_size_length =
      std::max(clip_bounds.width(), clip_bounds.height());

  painted_layer_.SetBounds(gfx::Rect(painted_size_length, painted_size_length));
  painted_layer_.SetFillsBoundsOpaquely(false);
  painted_layer_.set_delegate(&circle_layer_delegate_);
  painted_layer_.SetVisible(true);
  painted_layer_.SetOpacity(1.0);
  painted_layer_.SetMasksToBounds(false);
  painted_layer_.SetName("FloodFillInkDropRipple:PAINTED_LAYER");
  painted_layer_callback_subscription_ =
      painted_layer_.GetAnimator()->AddSequenceScheduledCallback(
          base::BindRepeating(
              &FloodFillInkDropRipple::OnLayerAnimationSequenceScheduled,
              base::Unretained(this)));

  root_layer_.Add(&painted_layer_);

  SetStateToHidden();
}

FloodFillInkDropRipple::FloodFillInkDropRipple(InkDropHost* ink_drop_host,
                                               const gfx::Size& host_size,
                                               const gfx::Point& center_point,
                                               SkColor color,
                                               float visible_opacity)
    : FloodFillInkDropRipple(ink_drop_host,
                             host_size,
                             gfx::Insets(),
                             center_point,
                             color,
                             visible_opacity) {}

FloodFillInkDropRipple::~FloodFillInkDropRipple() {
  // Explicitly aborting all the animations ensures all callbacks are invoked
  // while this instance still exists.
  AbortAllAnimations();
}

ui::Layer* FloodFillInkDropRipple::GetRootLayer() {
  return &root_layer_;
}

void FloodFillInkDropRipple::AnimateStateChange(
    InkDropState old_ink_drop_state,
    InkDropState new_ink_drop_state) {
  switch (new_ink_drop_state) {
    case InkDropState::HIDDEN:
      if (!IsVisible()) {
        SetStateToHidden();
      } else {
        AnimationBuilder()
            .SetPreemptionStrategy(
                ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
            .Once()
            .SetDuration(GetAnimationDuration(HIDDEN_FADE_OUT))
            .SetOpacity(&root_layer_, kHiddenOpacity, gfx::Tween::EASE_IN_OUT)
            .At(base::TimeDelta())
            .SetDuration(GetAnimationDuration(HIDDEN_TRANSFORM))
            .SetTransform(&painted_layer_, CalculateTransform(kMinRadius),
                          gfx::Tween::EASE_IN_OUT);
      }
      break;
    case InkDropState::ACTION_PENDING: {
      DLOG_IF(WARNING, InkDropState::HIDDEN != old_ink_drop_state)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      AnimationBuilder()
          .SetPreemptionStrategy(
              ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
          .Once()
          .SetDuration(GetAnimationDuration(ACTION_PENDING_FADE_IN))
          .SetOpacity(&root_layer_, visible_opacity_, gfx::Tween::EASE_IN)
          .At(base::TimeDelta())
          .SetDuration(GetAnimationDuration(ACTION_PENDING_TRANSFORM))
          .SetTransform(&painted_layer_, GetMaxSizeTargetTransform(),
                        gfx::Tween::FAST_OUT_SLOW_IN);
      break;
    }
    case InkDropState::ACTION_TRIGGERED: {
      DLOG_IF(WARNING, old_ink_drop_state != InkDropState::HIDDEN &&
                           old_ink_drop_state != InkDropState::ACTION_PENDING)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      if (old_ink_drop_state == InkDropState::HIDDEN) {
        AnimateStateChange(old_ink_drop_state, InkDropState::ACTION_PENDING);
      }
      AnimationBuilder()
          .SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION)
          .Once()
          .SetDuration(GetAnimationDuration(ACTION_TRIGGERED_FADE_OUT))
          .SetOpacity(&root_layer_, kHiddenOpacity, gfx::Tween::EASE_IN_OUT);
      break;
    }
    case InkDropState::ALTERNATE_ACTION_PENDING: {
      DLOG_IF(WARNING, InkDropState::ACTION_PENDING != old_ink_drop_state)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      AnimationBuilder()
          .SetPreemptionStrategy(
              ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
          .Once()
          .SetDuration(GetAnimationDuration(ALTERNATE_ACTION_PENDING))
          .SetOpacity(&root_layer_, visible_opacity_, gfx::Tween::EASE_IN)
          .SetTransform(&painted_layer_, GetMaxSizeTargetTransform(),
                        gfx::Tween::EASE_IN_OUT);
      break;
    }
    case InkDropState::ALTERNATE_ACTION_TRIGGERED:
      DLOG_IF(WARNING,
              InkDropState::ALTERNATE_ACTION_PENDING != old_ink_drop_state)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);
      AnimationBuilder()
          .SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION)
          .Once()
          .SetDuration(
              GetAnimationDuration(ALTERNATE_ACTION_TRIGGERED_FADE_OUT))
          .SetOpacity(&root_layer_, kHiddenOpacity, gfx::Tween::EASE_IN_OUT);
      break;
    case InkDropState::ACTIVATED: {
      if (old_ink_drop_state != InkDropState::ACTION_PENDING) {
        AnimationBuilder()
            .SetPreemptionStrategy(
                ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
            .Once()
            .SetDuration(GetAnimationDuration(ACTIVATED_FADE_IN))
            .SetOpacity(&root_layer_, visible_opacity_, gfx::Tween::EASE_IN)
            .At(base::TimeDelta())
            .SetDuration(GetAnimationDuration(ACTIVATED_TRANSFORM))
            .SetTransform(&painted_layer_, GetMaxSizeTargetTransform(),
                          gfx::Tween::EASE_IN_OUT);
      }
      break;
    }
    case InkDropState::DEACTIVATED:
      AnimationBuilder()
          .SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION)
          .Once()
          .SetDuration(GetAnimationDuration(DEACTIVATED_FADE_OUT))
          .SetOpacity(&root_layer_, kHiddenOpacity, gfx::Tween::EASE_IN_OUT);
      break;
  }
}

void FloodFillInkDropRipple::SetStateToActivated() {
  root_layer_.SetVisible(true);
  SetOpacity(visible_opacity_);
  painted_layer_.SetTransform(GetMaxSizeTargetTransform());
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

void FloodFillInkDropRipple::SetOpacity(float opacity) {
  root_layer_.SetOpacity(opacity);
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
  transform.PostConcat(GetTransformSubpixelCorrection(
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
base::TimeDelta FloodFillInkDropRipple::GetAnimationDuration(
    AnimationSubState state) {
  if (!PlatformStyle::kUseRipples ||
      !gfx::Animation::ShouldRenderRichAnimation() ||
      (GetInkDropHost() && GetInkDropHost()->GetMode() ==
                               InkDropHost::InkDropMode::ON_NO_ANIMATE)) {
    return base::TimeDelta();
  }

  // Override the requested state if needed.
  if (use_hide_transform_duration_for_hide_fade_out_ &&
      state == HIDDEN_FADE_OUT) {
    state = HIDDEN_TRANSFORM;
  }

  // Duration constants for InkDropSubAnimations. See the
  // InkDropStateSubAnimations enum documentation for more info.
  constexpr auto kAnimationDurationInMs = std::to_array<int>({
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
  });

  return base::Milliseconds(kAnimationDurationInMs[state] * duration_factor_);
}

void FloodFillInkDropRipple::OnLayerAnimationSequenceScheduled(
    ui::LayerAnimationSequence* sequence) {
  sequence->AddObserver(GetLayerAnimationObserver());
}

}  // namespace views
