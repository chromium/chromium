// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_SEQUENCE_BLOCK_H_
#define UI_VIEWS_ANIMATION_ANIMATION_SEQUENCE_BLOCK_H_

#include <map>
#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/animation/animation_key.h"
#include "ui/views/views_export.h"

namespace gfx {
class Rect;
class RoundedCornersF;
class LinearGradient;
}  // namespace gfx

namespace ui {
class InterpolatedTransform;
class Layer;
class LayerOwner;
}  // namespace ui

namespace views {

class AnimationBuilder;

// An animation sequence block is a single unit of a larger animation sequence,
// which has a start time, duration, and zero or more (target, property)
// animations. There may be multiple properties animating on a single target,
// and/or multiple targets animating, but the same property on the same target
// may only be animated at most once per block. Animations can be added by
// calling SetXXX(). Calling At(), Offset(), or Then() create a new block.
class VIEWS_EXPORT AnimationSequenceBlock {
 public:
  AnimationSequenceBlock(base::PassKey<AnimationBuilder> builder_key,
                         AnimationBuilder* owner,
                         base::TimeDelta start,
                         bool repeating);
  AnimationSequenceBlock(AnimationSequenceBlock&& other) = delete;
  AnimationSequenceBlock& operator=(AnimationSequenceBlock&& other) = delete;
  ~AnimationSequenceBlock();

  // Sets the duration of this block.  The duration may be set at most once and
  // will be zero if unspecified.
  AnimationSequenceBlock& SetDuration(base::TimeDelta duration);

  // Adds animation elements to this block.  Each (target, property) pair may be
  // added at most once.
  AnimationSequenceBlock& SetBounds(
      ui::Layer* target,
      const gfx::Rect& bounds,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetBounds(
      ui::LayerOwner* target,
      const gfx::Rect& bounds,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetBrightness(
      ui::Layer* target,
      float brightness,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetBrightness(
      ui::LayerOwner* target,
      float brightness,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetClipRect(
      ui::Layer* target,
      const gfx::Rect& clip_rect,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetClipRect(
      ui::LayerOwner* target,
      const gfx::Rect& clip_rect,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetColor(
      ui::Layer* target,
      SkColor color,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetColor(
      ui::LayerOwner* target,
      SkColor color,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetGrayscale(
      ui::Layer* target,
      float grayscale,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetGrayscale(
      ui::LayerOwner* target,
      float grayscale,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetOpacity(
      ui::Layer* target,
      float opacity,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetOpacity(
      ui::LayerOwner* target,
      float opacity,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetTransform(
      ui::Layer* target,
      gfx::Transform transform,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetTransform(
      ui::LayerOwner* target,
      gfx::Transform transform,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetRoundedCorners(
      ui::Layer* target,
      const gfx::RoundedCornersF& rounded_corners,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetRoundedCorners(
      ui::LayerOwner* target,
      const gfx::RoundedCornersF& rounded_corners,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetGradientMask(
      ui::Layer* target,
      const gfx::LinearGradient& gradient_mask,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetGradientMask(
      ui::LayerOwner* target,
      const gfx::LinearGradient& gradient_mask,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetVisibility(
      ui::Layer* target,
      bool visible,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetVisibility(
      ui::LayerOwner* target,
      bool visible,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);

  // NOTE: Generally an `ui::InterpolatedTransform` animation can be expressed
  // more simply as a `gfx::Transform` animation. As such, `SetTransform()` APIs
  // are preferred over `SetInterpolatedTransform()` APIs where possible.
  //
  // Exception #1: It may be preferable to use `SetInterpolatedTransform()` APIs
  // to animate overlapping transforms on the same `target`.
  //
  // Exception #2: It may be preferable to use `SetInterpolatedTransform()` APIs
  // when synchronous updates are required, as these APIs dispatch updates at
  // each animation step whereas `SetTransform()` APIs dispatch updates only at
  // animation start, complete, and abort.
  AnimationSequenceBlock& SetInterpolatedTransform(
      ui::Layer* target,
      std::unique_ptr<ui::InterpolatedTransform> interpolated_transform,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);
  AnimationSequenceBlock& SetInterpolatedTransform(
      ui::LayerOwner* target,
      std::unique_ptr<ui::InterpolatedTransform> interpolated_transform,
      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);

  // Creates a new block.
  AnimationSequenceBlock& At(base::TimeDelta since_sequence_start);
  AnimationSequenceBlock& Offset(base::TimeDelta since_last_block_start);
  AnimationSequenceBlock& Then();

 private:
  using AnimationValue =
      absl::variant<gfx::Rect,
                    float,
                    SkColor,
                    gfx::RoundedCornersF,
                    gfx::LinearGradient,
                    bool,
                    gfx::Transform,
                    std::unique_ptr<ui::InterpolatedTransform>>;

  // Data for the animation of a given AnimationKey.
  struct Element {
    Element(AnimationValue animation_value, gfx::Tween::Type tween_type);
    ~Element();
    Element(Element&&);
    Element& operator=(Element&&);
    AnimationValue animation_value_;
    gfx::Tween::Type tween_type_;
  };

  AnimationSequenceBlock& AddAnimation(AnimationKey key, Element element);

  // Called when the block is ended by At(), EndSequence(), or a variant
  // thereof. Converts `elements_` to LayerAnimationElements on the `owner_`.
  void TerminateBlock();

  base::PassKey<AnimationBuilder> builder_key_;
  raw_ptr<AnimationBuilder> owner_;
  base::TimeDelta start_;

  // The block duration.  This will contain nullopt (interpreted as zero) until
  // explicitly set by the caller, at which point it may not be reset.
  std::optional<base::TimeDelta> duration_;

  // The animation element data for this block. LayerAnimationElements are not
  // used directly because they must be created with a duration, whereas blocks
  // support setting the duration after creating elements. The conversion is
  // done in TerminateBlock().
  std::map<AnimationKey, Element> elements_;

  // Is this block part of a repeating sequence?
  bool repeating_ = false;

  // True when this block has been terminated or used to create another block.
  // At this point, it's an error to use the block further.
  bool finalized_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_SEQUENCE_BLOCK_H_
