// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DETAILS_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DETAILS_H_

#include <string.h>

#include "base/logging.h"
#include "ui/events/event_constants.h"
#include "ui/events/events_base_export.h"
#include "ui/events/types/scroll_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace ui {

struct EVENTS_BASE_EXPORT GestureEventDetails {
 public:
  GestureEventDetails();
  explicit GestureEventDetails(EventType type);

  GestureEventDetails(
      EventType type,
      float delta_x,
      float delta_y,
      ui::input_types::ScrollGranularity units =
          ui::input_types::ScrollGranularity::kScrollByPrecisePixel);

  // The caller is responsible for ensuring that the gesture data from |other|
  // is compatible and sufficient for that expected by gestures of |type|.
  GestureEventDetails(EventType type, const GestureEventDetails& other);

  EventType type() const { return type_; }

  GestureDeviceType device_type() const { return device_type_; }
  void set_device_type(GestureDeviceType device_type) {
    device_type_ = device_type;
  }

  bool is_source_touch_event_set_non_blocking() const {
    return is_source_touch_event_set_non_blocking_;
  }
  void set_is_source_touch_event_set_non_blocking(
      bool is_source_touch_event_set_non_blocking) {
    is_source_touch_event_set_non_blocking_ =
        is_source_touch_event_set_non_blocking;
  }

  EventPointerType primary_pointer_type() const {
    return primary_pointer_type_;
  }
  void set_primary_pointer_type(EventPointerType primary_pointer_type) {
    primary_pointer_type_ = primary_pointer_type;
  }

  int touch_points() const { return touch_points_; }
  void set_touch_points(int touch_points) {
    DCHECK_GT(touch_points, 0);
    touch_points_ = touch_points;
  }

  const gfx::Rect bounding_box() const {
    return ToEnclosingRect(bounding_box_);
  }

  const gfx::RectF& bounding_box_f() const {
    return bounding_box_;
  }

  void set_bounding_box(const gfx::RectF& box) { bounding_box_ = box; }

  float scroll_x_hint() const {
    DCHECK_EQ(ET_GESTURE_SCROLL_BEGIN, type_);
    return data_.scroll_begin.x_hint;
  }

  float scroll_y_hint() const {
    DCHECK_EQ(ET_GESTURE_SCROLL_BEGIN, type_);
    return data_.scroll_begin.y_hint;
  }

  ui::input_types::ScrollGranularity scroll_begin_units() const {
    DCHECK_EQ(ET_GESTURE_SCROLL_BEGIN, type_);
    return data_.scroll_begin.delta_hint_units;
  }

  float scroll_x() const {
    DCHECK_EQ(ET_GESTURE_SCROLL_UPDATE, type_);
    return data_.scroll_update.x;
  }

  float scroll_y() const {
    DCHECK_EQ(ET_GESTURE_SCROLL_UPDATE, type_);
    return data_.scroll_update.y;
  }

  ui::input_types::ScrollGranularity scroll_update_units() const {
    DCHECK_EQ(ET_GESTURE_SCROLL_UPDATE, type_);
    return data_.scroll_update.delta_units;
  }

  float velocity_x() const {
    DCHECK_EQ(ET_SCROLL_FLING_START, type_);
    return data_.fling_velocity.x;
  }

  float velocity_y() const {
    DCHECK_EQ(ET_SCROLL_FLING_START, type_);
    return data_.fling_velocity.y;
  }

  float first_finger_width() const {
    DCHECK_EQ(ET_GESTURE_TWO_FINGER_TAP, type_);
    return data_.first_finger_enclosing_rectangle.width;
  }

  float first_finger_height() const {
    DCHECK_EQ(ET_GESTURE_TWO_FINGER_TAP, type_);
    return data_.first_finger_enclosing_rectangle.height;
  }

  float scale() const {
    DCHECK_EQ(ET_GESTURE_PINCH_UPDATE, type_);
    return data_.scale;
  }

  bool swipe_left() const {
    DCHECK_EQ(ET_GESTURE_SWIPE, type_);
    return data_.swipe.left;
  }

  bool swipe_right() const {
    DCHECK_EQ(ET_GESTURE_SWIPE, type_);
    return data_.swipe.right;
  }

  bool swipe_up() const {
    DCHECK_EQ(ET_GESTURE_SWIPE, type_);
    return data_.swipe.up;
  }

  bool swipe_down() const {
    DCHECK_EQ(ET_GESTURE_SWIPE, type_);
    return data_.swipe.down;
  }

  int tap_count() const {
    DCHECK(type_ == ET_GESTURE_TAP ||
           type_ == ET_GESTURE_TAP_UNCONFIRMED ||
           type_ == ET_GESTURE_DOUBLE_TAP);
    return data_.tap_count;
  }

  void set_tap_count(int tap_count) {
    DCHECK_GE(tap_count, 0);
    DCHECK(type_ == ET_GESTURE_TAP ||
           type_ == ET_GESTURE_TAP_UNCONFIRMED ||
           type_ == ET_GESTURE_DOUBLE_TAP);
    data_.tap_count = tap_count;
  }

  void set_scale(float scale) {
    DCHECK_GE(scale, 0.0f);
    DCHECK_EQ(type_, ET_GESTURE_PINCH_UPDATE);
    data_.scale = scale;
  }

  // Supports comparison over internal structures for testing.
  bool operator==(const GestureEventDetails& other) const {
    return type_ == other.type_ &&
           !memcmp(&data_, &other.data_, sizeof(Details)) &&
           device_type_ == other.device_type_ &&
           touch_points_ == other.touch_points_ &&
           bounding_box_ == other.bounding_box_;
  }

 private:
  EventType type_;
  union Details {
    Details();
    struct {  // SCROLL start details.
      // Distance that caused the scroll to start.  Generally redundant with
      // the x/y values from the first scroll_update.
      float x_hint;
      float y_hint;
      ui::input_types::ScrollGranularity delta_hint_units;
    } scroll_begin;

    struct {  // SCROLL delta.
      float x;
      float y;
      ui::input_types::ScrollGranularity delta_units;
      // Whether any previous scroll update in the current scroll sequence was
      // suppressed because the underlying touch was consumed.
    } scroll_update;

    float scale;  // PINCH scale.

    struct {  // FLING velocity.
      float x;
      float y;
    } fling_velocity;

    // Dimensions of the first finger's enclosing rectangle for
    // TWO_FINGER_TAP.
    struct {
      float width;
      float height;
    } first_finger_enclosing_rectangle;

    struct {  // SWIPE direction.
      bool left;
      bool right;
      bool up;
      bool down;
    } swipe;

    // Tap information must be set for ET_GESTURE_TAP,
    // ET_GESTURE_TAP_UNCONFIRMED, and ET_GESTURE_DOUBLE_TAP events.
    int tap_count;  // TAP repeat count.
  } data_;

  GestureDeviceType device_type_;

  bool is_source_touch_event_set_non_blocking_ = false;

  // The pointer type for the first touch point in the gesture.
  EventPointerType primary_pointer_type_ =
      EventPointerType::POINTER_TYPE_UNKNOWN;

  int touch_points_;  // Number of active touch points in the gesture.

  // Bounding box is an axis-aligned rectangle that contains all the
  // enclosing rectangles of the touch-points in the gesture.
  gfx::RectF bounding_box_;
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DETAILS_H_
