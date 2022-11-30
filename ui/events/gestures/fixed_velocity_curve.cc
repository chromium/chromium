// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/fixed_velocity_curve.h"

namespace ui {

FixedVelocityCurve::FixedVelocityCurve(const gfx::Vector2dF& velocity,
                                       base::TimeTicks start_timestamp)
    : velocity_(velocity), start_timestamp_(start_timestamp) {}

FixedVelocityCurve::~FixedVelocityCurve() {}

// GestureCurve implementation.
bool FixedVelocityCurve::ComputeScrollOffset(base::TimeTicks time,
                                             gfx::Vector2dF* offset,
                                             gfx::Vector2dF* velocity) {
  *velocity = velocity_;

  const float kConstantMultiplier = 5000.0f;
  float multiplier =
      (time - start_timestamp_).InSecondsF() * kConstantMultiplier;
  *offset = gfx::ScaleVector2d(velocity_, multiplier);
  return true;
}

}  // namespace ui
