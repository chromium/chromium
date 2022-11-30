// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURES_FIXED_VELOCITY_CURVE_H_
#define UI_EVENTS_GESTURES_FIXED_VELOCITY_CURVE_H_

#include "base/time/time.h"
#include "ui/events/events_base_export.h"
#include "ui/events/gesture_curve.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

class EVENTS_BASE_EXPORT FixedVelocityCurve : public GestureCurve {
 public:
  FixedVelocityCurve(const gfx::Vector2dF& velocity,
                     base::TimeTicks start_timestamp);

  FixedVelocityCurve(const FixedVelocityCurve&) = delete;
  FixedVelocityCurve& operator=(const FixedVelocityCurve&) = delete;

  ~FixedVelocityCurve() override;

  // GestureCurve implementation.
  bool ComputeScrollOffset(base::TimeTicks time,
                           gfx::Vector2dF* offset,
                           gfx::Vector2dF* velocity) override;

 private:
  const gfx::Vector2dF velocity_;
  const base::TimeTicks start_timestamp_;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURES_FIXED_VELOCITY_CURVE_H_
