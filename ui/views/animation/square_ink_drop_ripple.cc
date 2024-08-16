// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/square_ink_drop_ripple.h"

#include <algorithm>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_sequence_block.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/view.h"

namespace views {

namespace {

// The minimum scale factor to use when scaling rectangle layers. Smaller values
// were causing visual anomalies.
constexpr float kMinimumRectScale = 0.0001f;

// The minimum scale factor to use when scaling circle layers. Smaller values
// were causing visual anomalies.
constexpr float kMinimumCircleScale = 0.001f;

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

  // The HIDDEN sub animation that transforms the shape to a |small_size_|
  // circle.
  HIDDEN_TRANSFORM,

  // ACTION_PENDING sub animations.

  // The ACTION_PENDING sub animation that fades in to the visible opacity.
  ACTION_PENDING_FADE_IN,

  // The ACTION_PENDING sub animation that transforms the shape to a
  // |large_size_| circle.
  ACTION_PENDING_TRANSFORM,

  // ACTION_TRIGGERED sub animations.

  // The ACTION_TRIGGERED sub animation that is fading out to a hidden opacity.
  ACTION_TRIGGERED_FADE_OUT,

  // The ACTION_TRIGGERED sub animation that transforms the shape to a
  // |large_size_|
  // circle.
  ACTION_TRIGGERED_TRANSFORM,

  // ALTERNATE_ACTION_PENDING sub animations.

  // The ALTERNATE_ACTION_PENDING animation has only one sub animation which
  // animates
  // to a |small_size_| rounded rectangle at visible opacity.
  ALTERNATE_ACTION_PENDING,

  // ALTERNATE_ACTION_TRIGGERED sub animations.

  // The ALTERNATE_ACTION_TRIGGERED sub animation that is fading out to a hidden
  // opacity.
  ALTERNATE_ACTION_TRIGGERED_FADE_OUT,

  // The ALTERNATE_ACTION_TRIGGERED sub animation that transforms the shape to a
  // |large_size_|
  // rounded rectangle.
  ALTERNATE_ACTION_TRIGGERED_TRANSFORM,

  // ACTIVATED sub animations.

  // The ACTIVATED sub animation that transforms the shape to a |large_size_|
  // circle. This is used when the ink drop is in a HIDDEN state prior to
  // animating to the ACTIVATED state.
  ACTIVATED_CIRCLE_TRANSFORM,

  // The ACTIVATED sub animation that transforms the shape to a |small_size_|
  // rounded rectangle.
  ACTIVATED_RECT_TRANSFORM,

  // DEACTIVATED sub animations.

  // The DEACTIVATED sub animation that is fading out to a hidden opacity.
  DEACTIVATED_FADE_OUT,

  // The DEACTIVATED sub animation that transforms the shape to a |large_size_|
  // rounded rectangle.
  DEACTIVATED_TRANSFORM,
};

// The scale factor used to burst the ACTION_TRIGGERED bubble as it fades out.
constexpr float kQuickActionBurstScale = 1.3f;

// Returns the InkDropState sub animation duration for the given |state|.
base::TimeDelta GetAnimationDuration(InkDropHost* ink_drop_host,
                                     InkDropSubAnimations state) {
  if (!PlatformStyle::kUseRipples ||
      !gfx::Animation::ShouldRenderRichAnimation() ||
      (ink_drop_host &&
       ink_drop_host->GetMode() == InkDropHost::InkDropMode::ON_NO_ANIMATE)) {
    return base::TimeDelta();
  }

  // Duration constants for InkDropStateSubAnimations. See the
  // InkDropStateSubAnimations enum documentation for more info.
  constexpr auto kAnimationDuration = std::to_array<base::TimeDelta>({
      base::Milliseconds(150),  // HIDDEN_FADE_OUT
      base::Milliseconds(200),  // HIDDEN_TRANSFORM
      base::TimeDelta(),        // ACTION_PENDING_FADE_IN
      base::Milliseconds(160),  // ACTION_PENDING_TRANSFORM
      base::Milliseconds(150),  // ACTION_TRIGGERED_FADE_OUT
      base::Milliseconds(160),  // ACTION_TRIGGERED_TRANSFORM
      base::Milliseconds(200),  // ALTERNATE_ACTION_PENDING
      base::Milliseconds(150),  // ALTERNAT..._TRIGGERED_FADE_OUT
      base::Milliseconds(200),  // ALTERNA..._TRIGGERED_TRANSFORM
      base::Milliseconds(200),  // ACTIVATED_CIRCLE_TRANSFORM
      base::Milliseconds(160),  // ACTIVATED_RECT_TRANSFORM
      base::Milliseconds(150),  // DEACTIVATED_FADE_OUT
      base::Milliseconds(200),  // DEACTIVATED_TRANSFORM
  });
  return kAnimationDuration[state];
}

}  // namespace

SquareInkDropRipple::SquareInkDropRipple(InkDropHost* ink_drop_host,
                                         const gfx::Size& large_size,
                                         int large_corner_radius,
                                         const gfx::Size& small_size,
                                         int small_corner_radius,
                                         const gfx::Point& center_point,
                                         SkColor color,
                                         float visible_opacity)
    : InkDropRipple(ink_drop_host),
      visible_opacity_(visible_opacity),
      large_size_(large_size),
      large_corner_radius_(large_corner_radius),
      small_size_(small_size),
      small_corner_radius_(small_corner_radius),
      center_point_(center_point),
      circle_layer_delegate_(new CircleLayerDelegate(
          color,
          std::min(large_size_.width(), large_size_.height()) / 2)),
      rect_layer_delegate_(
          new RectangleLayerDelegate(color, gfx::SizeF(large_size_))),
      root_layer_(ui::LAYER_NOT_DRAWN) {
  root_layer_.SetName("SquareInkDropRipple:ROOT_LAYER");

  for (size_t i = 0; i < PAINTED_SHAPE_COUNT; ++i) {
    AddPaintLayer(static_cast<PaintedShape>(i));
  }

  root_layer_.SetMasksToBounds(false);
  root_layer_.SetBounds(gfx::Rect(large_size_));
  root_callback_subscription_ =
      root_layer_.GetAnimator()->AddSequenceScheduledCallback(
          base::BindRepeating(
              &SquareInkDropRipple::OnLayerAnimationSequenceScheduled,
              base::Unretained(this)));

  SetStateToHidden();
}

SquareInkDropRipple::~SquareInkDropRipple() {
  // Explicitly aborting all the animations ensures all callbacks are invoked
  // while this instance still exists.
  AbortAllAnimations();
}

ui::Layer* SquareInkDropRipple::GetRootLayer() {
  return &root_layer_;
}

float SquareInkDropRipple::GetCurrentOpacity() const {
  return root_layer_.opacity();
}

std::string SquareInkDropRipple::ToLayerName(PaintedShape painted_shape) {
  switch (painted_shape) {
    case TOP_LEFT_CIRCLE:
      return "TOP_LEFT_CIRCLE";
    case TOP_RIGHT_CIRCLE:
      return "TOP_RIGHT_CIRCLE";
    case BOTTOM_RIGHT_CIRCLE:
      return "BOTTOM_RIGHT_CIRCLE";
    case BOTTOM_LEFT_CIRCLE:
      return "BOTTOM_LEFT_CIRCLE";
    case HORIZONTAL_RECT:
      return "HORIZONTAL_RECT";
    case VERTICAL_RECT:
      return "VERTICAL_RECT";
    case PAINTED_SHAPE_COUNT:
      NOTREACHED() << "The PAINTED_SHAPE_COUNT value should never be used.";
  }
  return "UNKNOWN";
}

void SquareInkDropRipple::AnimateStateChange(InkDropState old_ink_drop_state,
                                             InkDropState new_ink_drop_state) {
  InkDropTransforms transforms;
  AnimationBuilder builder;
  InkDropHost* host = GetInkDropHost();
  builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once();

  auto animate_to_transforms =
      [this](AnimationSequenceBlock& sequence,
             const InkDropTransforms& transforms,
             gfx::Tween::Type tween) -> AnimationSequenceBlock& {
    for (size_t i = 0; i < PAINTED_SHAPE_COUNT; ++i) {
      sequence.SetTransform(painted_layers_[i].get(), transforms[i], tween);
    }
    return sequence;
  };

  auto pending_animation =
      [this, &animate_to_transforms, host](
          AnimationSequenceBlock& sequence,
          const InkDropTransforms& transforms) -> AnimationSequenceBlock& {
    auto& new_sequence =
        sequence.SetDuration(GetAnimationDuration(host, ACTION_PENDING_FADE_IN))
            .SetOpacity(&root_layer_, visible_opacity_, gfx::Tween::EASE_IN)
            .Then()
            .SetDuration(GetAnimationDuration(host, ACTION_PENDING_TRANSFORM))
            .SetOpacity(&root_layer_, visible_opacity_, gfx::Tween::EASE_IN);
    animate_to_transforms(new_sequence, transforms, gfx::Tween::EASE_IN_OUT);
    return new_sequence;
  };

  switch (new_ink_drop_state) {
    case InkDropState::HIDDEN:
      if (!IsVisible()) {
        SetStateToHidden();
        break;
      } else {
        CalculateCircleTransforms(small_size_, &transforms);
        auto& sequence =
            builder.GetCurrentSequence()
                .SetDuration(GetAnimationDuration(host, HIDDEN_FADE_OUT))
                .SetOpacity(&root_layer_, kHiddenOpacity,
                            gfx::Tween::EASE_IN_OUT)
                .At(base::TimeDelta())
                .SetDuration(GetAnimationDuration(host, HIDDEN_TRANSFORM));
        animate_to_transforms(sequence, transforms, gfx::Tween::EASE_IN_OUT);
      }
      break;
    case InkDropState::ACTION_PENDING: {
      if (old_ink_drop_state == new_ink_drop_state)
        return;
      DLOG_IF(WARNING, InkDropState::HIDDEN != old_ink_drop_state)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      CalculateCircleTransforms(large_size_, &transforms);
      pending_animation(builder.GetCurrentSequence(), transforms);
      break;
    }
    case InkDropState::ACTION_TRIGGERED: {
      DLOG_IF(WARNING, old_ink_drop_state != InkDropState::HIDDEN &&
                           old_ink_drop_state != InkDropState::ACTION_PENDING)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      gfx::Size s = ScaleToRoundedSize(large_size_, kQuickActionBurstScale);
      CalculateCircleTransforms(s, &transforms);
      if (old_ink_drop_state == InkDropState::HIDDEN) {
        pending_animation(builder.GetCurrentSequence(), transforms).Then();
      } else {
        builder.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      }
      builder.GetCurrentSequence()
          .SetDuration(GetAnimationDuration(host, ACTION_TRIGGERED_FADE_OUT))
          .SetOpacity(&root_layer_, kHiddenOpacity, gfx::Tween::EASE_IN_OUT)
          .Offset(base::TimeDelta())
          .SetDuration(GetAnimationDuration(host, ACTION_TRIGGERED_TRANSFORM));
      animate_to_transforms(builder.GetCurrentSequence(), transforms,
                            gfx::Tween::EASE_IN_OUT);
      break;
    }
    case InkDropState::ALTERNATE_ACTION_PENDING: {
      DLOG_IF(WARNING, InkDropState::ACTION_PENDING != old_ink_drop_state)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      CalculateRectTransforms(small_size_, small_corner_radius_, &transforms);
      animate_to_transforms(
          builder.GetCurrentSequence()
              .SetDuration(GetAnimationDuration(host, ALTERNATE_ACTION_PENDING))
              .SetOpacity(&root_layer_, visible_opacity_, gfx::Tween::EASE_IN),
          transforms, gfx::Tween::EASE_IN_OUT);
      break;
    }
    case InkDropState::ALTERNATE_ACTION_TRIGGERED: {
      DLOG_IF(WARNING,
              InkDropState::ALTERNATE_ACTION_PENDING != old_ink_drop_state)
          << "Invalid InkDropState transition. old_ink_drop_state="
          << ToString(old_ink_drop_state)
          << " new_ink_drop_state=" << ToString(new_ink_drop_state);

      base::TimeDelta visible_duration =
          GetAnimationDuration(host, ALTERNATE_ACTION_TRIGGERED_TRANSFORM) -
          GetAnimationDuration(host, ALTERNATE_ACTION_TRIGGERED_FADE_OUT);
      CalculateRectTransforms(large_size_, large_corner_radius_, &transforms);
      builder.GetCurrentSequence()
          .SetDuration(visible_duration)
          .SetOpacity(&root_layer_, visible_opacity_, gfx::Tween::EASE_IN_OUT)
          .Then()
          .SetDuration(
              GetAnimationDuration(host, ALTERNATE_ACTION_TRIGGERED_FADE_OUT))
          .SetOpacity(&root_layer_, kHiddenOpacity, gfx::Tween::EASE_IN_OUT)
          .At(base::TimeDelta())
          .SetDuration(
              GetAnimationDuration(host, ALTERNATE_ACTION_TRIGGERED_TRANSFORM));
      animate_to_transforms(builder.GetCurrentSequence(), transforms,
                            gfx::Tween::EASE_IN_OUT);
      break;
    }
    case InkDropState::ACTIVATED: {
      // Animate the opacity so that it cancels any opacity animations already
      // in progress.
      builder.GetCurrentSequence()
          .SetDuration(base::TimeDelta())
          .SetOpacity(&root_layer_, visible_opacity_, gfx::Tween::EASE_IN_OUT)
          .Then();

      if (old_ink_drop_state == InkDropState::HIDDEN) {
        CalculateCircleTransforms(large_size_, &transforms);
        animate_to_transforms(
            builder.GetCurrentSequence().SetDuration(
                GetAnimationDuration(host, ACTIVATED_CIRCLE_TRANSFORM)),
            transforms, gfx::Tween::EASE_IN_OUT)
            .Then();
      } else if (old_ink_drop_state == InkDropState::ACTION_PENDING) {
        builder.SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      }

      GetActivatedTargetTransforms(&transforms);
      animate_to_transforms(
          builder.GetCurrentSequence().SetDuration(
              GetAnimationDuration(host, ACTIVATED_RECT_TRANSFORM)),
          transforms, gfx::Tween::EASE_IN_OUT);
      break;
    }
    case InkDropState::DEACTIVATED: {
      base::TimeDelta visible_duration =
          GetAnimationDuration(host, DEACTIVATED_TRANSFORM) -
          GetAnimationDuration(host, DEACTIVATED_FADE_OUT);
      GetDeactivatedTargetTransforms(&transforms);
      builder.GetCurrentSequence()
          .SetDuration(visible_duration)
          .SetOpacity(&root_layer_, visible_opacity_, gfx::Tween::EASE_IN_OUT)
          .Then()
          .SetDuration(GetAnimationDuration(host, DEACTIVATED_FADE_OUT))
          .SetOpacity(&root_layer_, kHiddenOpacity, gfx::Tween::EASE_IN_OUT)
          .At(base::TimeDelta());
      animate_to_transforms(
          builder.GetCurrentSequence().SetDuration(
              GetAnimationDuration(host, DEACTIVATED_TRANSFORM)),
          transforms, gfx::Tween::EASE_IN_OUT);
      break;
    }
  }
}

void SquareInkDropRipple::SetStateToActivated() {
  root_layer_.SetVisible(true);
  SetOpacity(visible_opacity_);
  InkDropTransforms transforms;
  GetActivatedTargetTransforms(&transforms);
  SetTransforms(transforms);
}

void SquareInkDropRipple::SetStateToHidden() {
  InkDropTransforms transforms;
  // Use non-zero size to avoid visual anomalies.
  CalculateCircleTransforms(gfx::Size(1, 1), &transforms);
  SetTransforms(transforms);
  root_layer_.SetOpacity(InkDropRipple::kHiddenOpacity);
  root_layer_.SetVisible(false);
}

void SquareInkDropRipple::AbortAllAnimations() {
  root_layer_.GetAnimator()->AbortAllAnimations();
  for (auto& painted_layer : painted_layers_)
    painted_layer->GetAnimator()->AbortAllAnimations();
}

void SquareInkDropRipple::SetTransforms(const InkDropTransforms transforms) {
  for (size_t i = 0; i < PAINTED_SHAPE_COUNT; ++i) {
    painted_layers_[i]->SetTransform(transforms[i]);
  }
}

void SquareInkDropRipple::SetOpacity(float opacity) {
  root_layer_.SetOpacity(opacity);
}

void SquareInkDropRipple::CalculateCircleTransforms(
    const gfx::Size& size,
    InkDropTransforms* transforms_out) const {
  CalculateRectTransforms(size, std::min(size.width(), size.height()) / 2.0f,
                          transforms_out);
}

void SquareInkDropRipple::CalculateRectTransforms(
    const gfx::Size& desired_size,
    float corner_radius,
    InkDropTransforms* transforms_out) const {
  DCHECK_GE(desired_size.width() / 2.0f, corner_radius)
      << "The circle's diameter should not be greater than the total width.";
  DCHECK_GE(desired_size.height() / 2.0f, corner_radius)
      << "The circle's diameter should not be greater than the total height.";

  gfx::SizeF size(desired_size);
  // This function can be called before the layer's been added to a view,
  // either at construction time or in tests.
  if (root_layer_.GetCompositor()) {
    // Modify |desired_size| so that the ripple aligns to pixel bounds.
    const float dsf = root_layer_.GetCompositor()->device_scale_factor();
    gfx::RectF ripple_bounds((gfx::PointF(center_point_)), gfx::SizeF());
    ripple_bounds.Inset(-gfx::InsetsF::VH(desired_size.height() / 2.0f,
                                          desired_size.width() / 2.0f));
    ripple_bounds.Scale(dsf);
    ripple_bounds = gfx::RectF(gfx::ToEnclosingRect(ripple_bounds));
    ripple_bounds.InvScale(dsf);
    size = ripple_bounds.size();
  }

  // The shapes are drawn such that their center points are not at the origin.
  // Thus we use the CalculateCircleTransform() and CalculateRectTransform()
  // methods to calculate the complex Transforms.

  const float circle_scale = std::max(
      kMinimumCircleScale,
      corner_radius / static_cast<float>(circle_layer_delegate_->radius()));

  const float circle_target_x_offset = size.width() / 2.0f - corner_radius;
  const float circle_target_y_offset = size.height() / 2.0f - corner_radius;

  (*transforms_out)[TOP_LEFT_CIRCLE] = CalculateCircleTransform(
      circle_scale, -circle_target_x_offset, -circle_target_y_offset);
  (*transforms_out)[TOP_RIGHT_CIRCLE] = CalculateCircleTransform(
      circle_scale, circle_target_x_offset, -circle_target_y_offset);
  (*transforms_out)[BOTTOM_RIGHT_CIRCLE] = CalculateCircleTransform(
      circle_scale, circle_target_x_offset, circle_target_y_offset);
  (*transforms_out)[BOTTOM_LEFT_CIRCLE] = CalculateCircleTransform(
      circle_scale, -circle_target_x_offset, circle_target_y_offset);

  const float rect_delegate_width = rect_layer_delegate_->size().width();
  const float rect_delegate_height = rect_layer_delegate_->size().height();

  (*transforms_out)[HORIZONTAL_RECT] = CalculateRectTransform(
      std::max(kMinimumRectScale, size.width() / rect_delegate_width),
      std::max(kMinimumRectScale,
               (size.height() - 2.0f * corner_radius) / rect_delegate_height));

  (*transforms_out)[VERTICAL_RECT] = CalculateRectTransform(
      std::max(kMinimumRectScale,
               (size.width() - 2.0f * corner_radius) / rect_delegate_width),
      std::max(kMinimumRectScale, size.height() / rect_delegate_height));
}

gfx::Transform SquareInkDropRipple::CalculateCircleTransform(
    float scale,
    float target_center_x,
    float target_center_y) const {
  gfx::Transform transform;
  // Offset for the center point of the ripple.
  transform.Translate(center_point_.x(), center_point_.y());
  // Move circle to target.
  transform.Translate(target_center_x, target_center_y);
  transform.Scale(scale, scale);
  // Align center point of the painted circle.
  const gfx::Vector2dF circle_center_offset =
      circle_layer_delegate_->GetCenteringOffset();
  transform.Translate(-circle_center_offset.x(), -circle_center_offset.y());
  return transform;
}

gfx::Transform SquareInkDropRipple::CalculateRectTransform(
    float x_scale,
    float y_scale) const {
  gfx::Transform transform;
  transform.Translate(center_point_.x(), center_point_.y());
  transform.Scale(x_scale, y_scale);
  const gfx::Vector2dF rect_center_offset =
      rect_layer_delegate_->GetCenteringOffset();
  transform.Translate(-rect_center_offset.x(), -rect_center_offset.y());
  return transform;
}

void SquareInkDropRipple::GetCurrentTransforms(
    InkDropTransforms* transforms_out) const {
  for (size_t i = 0; i < PAINTED_SHAPE_COUNT; ++i) {
    (*transforms_out)[i] = painted_layers_[i]->transform();
  }
}

void SquareInkDropRipple::GetActivatedTargetTransforms(
    InkDropTransforms* transforms_out) const {
  switch (activated_shape_) {
    case ActivatedShape::kCircle:
      CalculateCircleTransforms(small_size_, transforms_out);
      break;
    case ActivatedShape::kRoundedRect:
      CalculateRectTransforms(small_size_, small_corner_radius_,
                              transforms_out);
      break;
  }
}

void SquareInkDropRipple::GetDeactivatedTargetTransforms(
    InkDropTransforms* transforms_out) const {
  switch (activated_shape_) {
    case ActivatedShape::kCircle:
      CalculateCircleTransforms(large_size_, transforms_out);
      break;
    case ActivatedShape::kRoundedRect:
      CalculateRectTransforms(large_size_, small_corner_radius_,
                              transforms_out);
      break;
  }
}

void SquareInkDropRipple::AddPaintLayer(PaintedShape painted_shape) {
  ui::LayerDelegate* delegate = nullptr;
  switch (painted_shape) {
    case TOP_LEFT_CIRCLE:
    case TOP_RIGHT_CIRCLE:
    case BOTTOM_RIGHT_CIRCLE:
    case BOTTOM_LEFT_CIRCLE:
      delegate = circle_layer_delegate_.get();
      break;
    case HORIZONTAL_RECT:
    case VERTICAL_RECT:
      delegate = rect_layer_delegate_.get();
      break;
    case PAINTED_SHAPE_COUNT:
      NOTREACHED() << "PAINTED_SHAPE_COUNT is not an actual shape type.";
  }

  ui::Layer* layer = new ui::Layer();
  root_layer_.Add(layer);

  layer->SetBounds(gfx::Rect(large_size_));
  layer->SetFillsBoundsOpaquely(false);
  layer->set_delegate(delegate);
  layer->SetVisible(true);
  layer->SetOpacity(1.0);
  layer->SetMasksToBounds(false);
  layer->SetName("PAINTED_SHAPE_COUNT:" + ToLayerName(painted_shape));
  callback_subscriptions_[painted_shape] =
      layer->GetAnimator()->AddSequenceScheduledCallback(base::BindRepeating(
          &SquareInkDropRipple::OnLayerAnimationSequenceScheduled,
          base::Unretained(this)));

  painted_layers_[painted_shape].reset(layer);
}

void SquareInkDropRipple::OnLayerAnimationSequenceScheduled(
    ui::LayerAnimationSequence* sequence) {
  sequence->AddObserver(GetLayerAnimationObserver());
}

}  // namespace views
