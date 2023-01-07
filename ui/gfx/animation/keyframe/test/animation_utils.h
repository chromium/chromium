// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ANIMATION_KEYFRAME_TEST_ANIMATION_UTILS_H_
#define UI_GFX_ANIMATION_KEYFRAME_TEST_ANIMATION_UTILS_H_

#include <vector>

#include "ui/gfx/animation/keyframe/keyframe_model.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace gfx {

std::unique_ptr<KeyframeModel> CreateTransformAnimation(
    TransformAnimationCurve::Target* target,
    int id,
    int property_id,
    const TransformOperations& from,
    const TransformOperations& to,
    base::TimeDelta duration);

std::unique_ptr<KeyframeModel> CreateSizeAnimation(
    SizeAnimationCurve::Target* target,
    int id,
    int property_id,
    const SizeF& from,
    const SizeF& to,
    base::TimeDelta duration);

std::unique_ptr<KeyframeModel> CreateFloatAnimation(
    FloatAnimationCurve::Target* target,
    int id,
    int property_id,
    float from,
    float to,
    base::TimeDelta duration);

std::unique_ptr<KeyframeModel> CreateColorAnimation(
    ColorAnimationCurve::Target* target,
    int id,
    int property_id,
    SkColor from,
    SkColor to,
    base::TimeDelta duration);

base::TimeTicks MicrosecondsToTicks(uint64_t us);
base::TimeDelta MicrosecondsToDelta(uint64_t us);

base::TimeTicks MsToTicks(uint64_t us);
base::TimeDelta MsToDelta(uint64_t us);

}  // namespace gfx

#endif  // UI_GFX_ANIMATION_KEYFRAME_TEST_ANIMATION_UTILS_H_
