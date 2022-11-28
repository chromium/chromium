// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ANDROID_GESTURE_EVENT_ANDROID_H_
#define UI_EVENTS_ANDROID_GESTURE_EVENT_ANDROID_H_

#include <memory>

#include "ui/events/events_export.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// Event class used to carry the info that match the blink::WebGestureEvent.
// This was devised because we can't use the blink type in ViewAndroid tree
// since hit testing requires templated things to modify these events.
// All coordinates are in DIPs.
class EVENTS_EXPORT GestureEventAndroid {
 public:
  GestureEventAndroid(int type,
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
                      bool prevent_boosting);

  GestureEventAndroid(const GestureEventAndroid&) = delete;
  GestureEventAndroid& operator=(const GestureEventAndroid&) = delete;

  ~GestureEventAndroid();

  int type() const { return type_; }
  const gfx::PointF& location() const { return location_; }
  const gfx::PointF& screen_location() const { return screen_location_; }
  long time() const { return time_ms_; }
  float scale() const { return scale_; }
  float delta_x() const { return delta_x_; }
  float delta_y() const { return delta_y_; }
  float velocity_x() const { return velocity_x_; }
  float velocity_y() const { return velocity_y_; }
  bool target_viewport() const { return target_viewport_; }
  bool synthetic_scroll() const { return synthetic_scroll_; }
  bool prevent_boosting() const { return prevent_boosting_; }

  // Creates a new GestureEventAndroid instance different from |this| only by
  // its location.
  std::unique_ptr<GestureEventAndroid> CreateFor(
      const gfx::PointF& new_location) const;

 private:
  int type_;
  gfx::PointF location_;
  gfx::PointF screen_location_;
  long time_ms_;

  float scale_;
  float delta_x_;
  float delta_y_;
  float velocity_x_;
  float velocity_y_;
  bool target_viewport_;
  bool synthetic_scroll_;

  // Used by fling cancel. If true, this gesture will never attempt to boost an
  // existing fling. It will immediately cancel an existing fling.
  bool prevent_boosting_;
};

}  // namespace ui

#endif  // UI_EVENTS_ANDROID_GESTURE_EVENT_ANDROID_H_
