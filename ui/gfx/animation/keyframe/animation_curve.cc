// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/animation/keyframe/animation_curve.h"

#include "base/check.h"
#include "ui/gfx/geometry/transform_operations.h"

namespace gfx {

bool AnimationCurve::PreservesAxisAlignment() const {
  return true;
}

bool AnimationCurve::MaximumScale(float* max_scale) const {
  return false;
}

base::TimeDelta AnimationCurve::TickInterval() const {
  return base::TimeDelta();
}

#define DEFINE_ANIMATION_CURVE(Name, CurveType)                               \
  void Name##AnimationCurve::Tick(                                            \
      base::TimeDelta t, int property_id, KeyframeModel* keyframe_model,      \
      gfx::TimingFunction::LimitDirection limit_direction) const {            \
    if (target_) {                                                            \
      target_->On##Name##Animated(GetTransformedValue(t, limit_direction),    \
                                  property_id, keyframe_model);               \
    }                                                                         \
  }                                                                           \
  int Name##AnimationCurve::Type() const {                                    \
    return AnimationCurve::CurveType;                                         \
  }                                                                           \
  const char* Name##AnimationCurve::TypeName() const {                        \
    return #Name;                                                             \
  }                                                                           \
  const Name##AnimationCurve* Name##AnimationCurve::To##Name##AnimationCurve( \
      const AnimationCurve* c) {                                              \
    DCHECK_EQ(AnimationCurve::CurveType, c->Type());                          \
    return static_cast<const Name##AnimationCurve*>(c);                       \
  }                                                                           \
  Name##AnimationCurve* Name##AnimationCurve::To##Name##AnimationCurve(       \
      AnimationCurve* c) {                                                    \
    DCHECK_EQ(AnimationCurve::CurveType, c->Type());                          \
    return static_cast<Name##AnimationCurve*>(c);                             \
  }

DEFINE_ANIMATION_CURVE(Transform, TRANSFORM)
DEFINE_ANIMATION_CURVE(Float, FLOAT)
DEFINE_ANIMATION_CURVE(Size, SIZE)
DEFINE_ANIMATION_CURVE(Color, COLOR)
DEFINE_ANIMATION_CURVE(Rect, RECT)

}  // namespace gfx
