// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_DID_OVERSCROLL_PARAMS_H_
#define UI_EVENTS_BLINK_DID_OVERSCROLL_PARAMS_H_

#include "cc/input/overscroll_behavior.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

struct DidOverscrollParams {
  gfx::Vector2dF accumulated_overscroll;
  gfx::Vector2dF latest_overscroll_delta;
  gfx::Vector2dF current_fling_velocity;
  gfx::PointF causal_event_viewport_point;
  cc::OverscrollBehavior overscroll_behavior;
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_DID_OVERSCROLL_PARAMS_H_
