// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_SEQUENCE_BLOCK_H_
#define UI_VIEWS_ANIMATION_ANIMATION_SEQUENCE_BLOCK_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/animation/animation_key.h"
#include "ui/views/views_export.h"

// This AnimationBuilder API is currently in the experimental phase and only
// used within ui/views/examples/.

namespace gfx {
class Rect;
class RoundedCornersF;
}  // namespace gfx

namespace ui {
class InterpolatedTransform;
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
                         base::TimeDelta start);
  AnimationSequenceBlock(AnimationSequenceBlock&&);
  AnimationSequenceBlock& operator=(AnimationSequenceBlock&&);
  ~AnimationSequenceBlock();

  // Sets the duration of this block.  The duration may be set at most once and
  // will be zero if unspecified.
  AnimationSequenceBlock& SetDuration(base::TimeDelta duration);

  // Adds animation elements to this block.  Each (target, property) pair may be
  // added at most once.
  AnimationSequenceBlock& SetBounds(ui::LayerOwner* target,
                                    const gfx::Rect& bounds);
  AnimationSequenceBlock& SetBrightness(ui::LayerOwner* target,
                                        float brightness);
  AnimationSequenceBlock& SetClipRect(ui::LayerOwner* target,
                                      const gfx::Rect& clip_rect);
  AnimationSequenceBlock& SetColor(ui::LayerOwner* target, SkColor color);
  AnimationSequenceBlock& SetGrayscale(ui::LayerOwner* target, float grayscale);
  AnimationSequenceBlock& SetOpacity(ui::LayerOwner* target, float opacity);
  AnimationSequenceBlock& SetInterpolatedTransform(
      ui::LayerOwner* target,
      std::unique_ptr<ui::InterpolatedTransform> interpolated_transform);
  AnimationSequenceBlock& SetRoundedCorners(
      ui::LayerOwner* target,
      const gfx::RoundedCornersF& rounded_corners);
  AnimationSequenceBlock& SetVisibility(ui::LayerOwner* target, bool visible);

  // Creates a new block.
  AnimationSequenceBlock At(base::TimeDelta since_sequence_start);
  AnimationSequenceBlock Offset(base::TimeDelta since_last_block_start);
  AnimationSequenceBlock Then();

 private:
  // Data for the animation of a given AnimationKey.
  using Element = absl::variant<gfx::Rect,
                                float,
                                SkColor,
                                std::unique_ptr<ui::InterpolatedTransform>,
                                gfx::RoundedCornersF,
                                bool>;

  AnimationSequenceBlock& AddAnimation(AnimationKey key, Element element);

  // Called when the block is ended by At(), EndSequence(), or a variant
  // thereof. Converts `elements_` to LayerAnimationElements on the `owner_`.
  void TerminateBlock();

  base::PassKey<AnimationBuilder> builder_key_;
  AnimationBuilder* owner_;
  base::TimeDelta start_;

  // The block duration.  This will contain nullopt (interpreted as zero) until
  // explicitly set by the caller, at which point it may not be reset.
  absl::optional<base::TimeDelta> duration_;

  // The animation element data for this block. LayerAnimationElements are not
  // used directly because they must be created with a duration, whereas blocks
  // support setting the duration after creating elements. The conversion is
  // done in TerminateBlock().
  std::map<AnimationKey, Element> elements_;

  // Whether this is the last block in the sequence.
  bool is_terminal_block_ = true;
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_SEQUENCE_BLOCK_H_
