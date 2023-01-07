// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/android/gesture_event_android.h"

#include <memory>

#include "ui/gfx/geometry/point_f.h"

namespace ui {

GestureEventAndroid::GestureEventAndroid(int type,
                                         const gfx::PointF& location,
                                         const gfx::PointF& screen_location,
                                         long time_ms,
                                         float scale,
                                         float delta_x,
                                         float delta_y,
                                         float velocity_x,
                                         float velocity_y,
                                         bool target_viewport,
                                         bool synthetic_scroll,
                                         bool prevent_boosting)
    : type_(type),
      location_(location),
      screen_location_(screen_location),
      time_ms_(time_ms),
      scale_(scale),
      delta_x_(delta_x),
      delta_y_(delta_y),
      velocity_x_(velocity_x),
      velocity_y_(velocity_y),
      target_viewport_(target_viewport),
      synthetic_scroll_(synthetic_scroll),
      prevent_boosting_(prevent_boosting) {}

GestureEventAndroid::~GestureEventAndroid() {}

std::unique_ptr<GestureEventAndroid> GestureEventAndroid::CreateFor(
    const gfx::PointF& new_location) const {
  auto offset = new_location - location_;
  gfx::PointF new_screen_location = screen_location_ + offset;
  return std::make_unique<GestureEventAndroid>(
      type_, new_location, new_screen_location, time_ms_, scale_, delta_x_,
      delta_y_, velocity_x_, velocity_y_, target_viewport_, synthetic_scroll_,
      prevent_boosting_);
}

}  // namespace ui
