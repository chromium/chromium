// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/animation_sequence_block.h"

#include <map>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/interpolated_transform.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/animation_key.h"

namespace views {

using PassKey = base::PassKey<AnimationSequenceBlock>;

AnimationSequenceBlock::AnimationSequenceBlock(
    base::PassKey<AnimationBuilder> builder_key,
    AnimationBuilder* owner,
    base::TimeDelta start)
    : builder_key_(builder_key), owner_(owner), start_(start) {}

AnimationSequenceBlock::AnimationSequenceBlock(AnimationSequenceBlock&&) =
    default;

AnimationSequenceBlock& AnimationSequenceBlock::operator=(
    AnimationSequenceBlock&&) = default;

AnimationSequenceBlock::~AnimationSequenceBlock() {
  if (is_terminal_block_) {
    TerminateBlock();
    owner_->TerminateSequence(PassKey());
  }
}

AnimationSequenceBlock& AnimationSequenceBlock::SetDuration(
    base::TimeDelta duration) {
  DCHECK(!duration_.has_value()) << "Duration may be set at most once.";
  duration_ = duration;
  return *this;
}

AnimationSequenceBlock& AnimationSequenceBlock::SetBounds(
    ui::LayerOwner* target,
    const gfx::Rect& bounds) {
  return AddAnimation({target, ui::LayerAnimationElement::BOUNDS}, bounds);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetBrightness(
    ui::LayerOwner* target,
    float brightness) {
  return AddAnimation({target, ui::LayerAnimationElement::BRIGHTNESS},
                      brightness);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetClipRect(
    ui::LayerOwner* target,
    const gfx::Rect& clip_rect) {
  return AddAnimation({target, ui::LayerAnimationElement::CLIP}, clip_rect);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetColor(ui::LayerOwner* target,
                                                         SkColor color) {
  return AddAnimation({target, ui::LayerAnimationElement::COLOR}, color);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetGrayscale(
    ui::LayerOwner* target,
    float grayscale) {
  return AddAnimation({target, ui::LayerAnimationElement::GRAYSCALE},
                      grayscale);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetOpacity(
    ui::LayerOwner* target,
    float opacity) {
  return AddAnimation({target, ui::LayerAnimationElement::OPACITY}, opacity);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetInterpolatedTransform(
    ui::LayerOwner* target,
    std::unique_ptr<ui::InterpolatedTransform> interpolated_transform) {
  return AddAnimation({target, ui::LayerAnimationElement::TRANSFORM},
                      std::move(interpolated_transform));
}

AnimationSequenceBlock& AnimationSequenceBlock::SetRoundedCorners(
    ui::LayerOwner* target,
    const gfx::RoundedCornersF& rounded_corners) {
  return AddAnimation({target, ui::LayerAnimationElement::ROUNDED_CORNERS},
                      rounded_corners);
}

AnimationSequenceBlock& AnimationSequenceBlock::SetVisibility(
    ui::LayerOwner* target,
    bool visible) {
  return AddAnimation({target, ui::LayerAnimationElement::VISIBILITY}, visible);
}

AnimationSequenceBlock AnimationSequenceBlock::At(
    base::TimeDelta since_sequence_start) {
  TerminateBlock();
  is_terminal_block_ = false;
  return AnimationSequenceBlock(builder_key_, owner_, since_sequence_start);
}

AnimationSequenceBlock AnimationSequenceBlock::Offset(
    base::TimeDelta since_last_block_start) {
  return At(start_ + since_last_block_start);
}

AnimationSequenceBlock AnimationSequenceBlock::Then() {
  return Offset(duration_.value_or(base::TimeDelta()));
}

AnimationSequenceBlock& AnimationSequenceBlock::AddAnimation(AnimationKey key,
                                                             Element element) {
  const auto result =
      elements_.insert(std::make_pair(std::move(key), std::move(element)));
  DCHECK(result.second) << "Animate (target, property) at most once per block.";
  return *this;
}

void AnimationSequenceBlock::TerminateBlock() {
  for (auto& pair : elements_) {
    std::unique_ptr<ui::LayerAnimationElement> element;
    const auto duration = duration_.value_or(base::TimeDelta());
    switch (pair.first.property) {
      case ui::LayerAnimationElement::TRANSFORM:
        element = ui::LayerAnimationElement::CreateInterpolatedTransformElement(
            absl::get<std::unique_ptr<ui::InterpolatedTransform>>(
                std::move(pair.second)),
            duration);
        break;
      case ui::LayerAnimationElement::BOUNDS:
        element = ui::LayerAnimationElement::CreateBoundsElement(
            absl::get<gfx::Rect>(pair.second), duration);
        break;
      case ui::LayerAnimationElement::OPACITY:
        element = ui::LayerAnimationElement::CreateOpacityElement(
            absl::get<float>(pair.second), duration);
        break;
      case ui::LayerAnimationElement::VISIBILITY:
        element = ui::LayerAnimationElement::CreateVisibilityElement(
            absl::get<bool>(pair.second), duration);
        break;
      case ui::LayerAnimationElement::BRIGHTNESS:
        element = ui::LayerAnimationElement::CreateBrightnessElement(
            absl::get<float>(pair.second), duration);
        break;
      case ui::LayerAnimationElement::GRAYSCALE:
        element = ui::LayerAnimationElement::CreateGrayscaleElement(
            absl::get<float>(pair.second), duration);
        break;
      case ui::LayerAnimationElement::COLOR:
        element = ui::LayerAnimationElement::CreateColorElement(
            absl::get<SkColor>(pair.second), duration);
        break;
      case ui::LayerAnimationElement::CLIP:
        element = ui::LayerAnimationElement::CreateClipRectElement(
            absl::get<gfx::Rect>(pair.second), duration);
        break;
      case ui::LayerAnimationElement::ROUNDED_CORNERS:
        element = ui::LayerAnimationElement::CreateRoundedCornersElement(
            absl::get<gfx::RoundedCornersF>(pair.second), duration);
        break;
      default:
        NOTREACHED();
    }
    owner_->AddLayerAnimationElement(PassKey(), pair.first, start_,
                                     std::move(element));
  }

  elements_.clear();
}

}  // namespace views
