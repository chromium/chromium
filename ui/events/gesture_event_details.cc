// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_event_details.h"

#include <ostream>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"

namespace ui {

GestureEventDetails::GestureEventDetails()
    : type_(EventType::kUnknown),
      device_type_(GestureDeviceType::DEVICE_UNKNOWN),
      touch_points_(0) {}

GestureEventDetails::GestureEventDetails(ui::EventType type)
    : type_(type),
      device_type_(GestureDeviceType::DEVICE_UNKNOWN),
      touch_points_(1) {
  DCHECK_GE(type, EventType::kGestureTypeStart);
  DCHECK_LE(type, EventType::kGestureTypeEnd);
}

GestureEventDetails::GestureEventDetails(ui::EventType type,
                                         float delta_x,
                                         float delta_y,
                                         ui::ScrollGranularity units)
    : type_(type),
      device_type_(GestureDeviceType::DEVICE_UNKNOWN),
      touch_points_(1) {
  DCHECK_GE(type, EventType::kGestureTypeStart);
  DCHECK_LE(type, EventType::kGestureTypeEnd);
  switch (type_) {
    case ui::EventType::kGestureScrollBegin:
      data_.scroll_begin.x_hint = delta_x;
      data_.scroll_begin.y_hint = delta_y;
      data_.scroll_begin.delta_hint_units = units;
      break;

    case ui::EventType::kGestureScrollUpdate:
      data_.scroll_update.x = delta_x;
      data_.scroll_update.y = delta_y;
      data_.scroll_update.delta_units = units;
      break;

    case ui::EventType::kScrollFlingStart:
      data_.fling_velocity.x = delta_x;
      data_.fling_velocity.y = delta_y;
      break;

    case ui::EventType::kGestureTwoFingerTap:
      data_.first_finger_enclosing_rectangle.width = delta_x;
      data_.first_finger_enclosing_rectangle.height = delta_y;
      break;

    case ui::EventType::kGestureSwipe:
      data_.swipe.left = delta_x < 0;
      data_.swipe.right = delta_x > 0;
      data_.swipe.up = delta_y < 0;
      data_.swipe.down = delta_y > 0;
      break;

    default:
      NOTREACHED_IN_MIGRATION() << "Invalid event type for constructor: "
                                << base::to_underlying(type);
  }
}

GestureEventDetails::GestureEventDetails(ui::EventType type,
                                         const GestureEventDetails& other)
    : type_(type),
      data_(other.data_),
      device_type_(other.device_type_),
      primary_pointer_type_(other.primary_pointer_type_),
      primary_unique_touch_event_id_(other.primary_unique_touch_event_id_),
      touch_points_(other.touch_points_),
      bounding_box_(other.bounding_box_) {
  DCHECK_GE(type, EventType::kGestureTypeStart);
  DCHECK_LE(type, EventType::kGestureTypeEnd);
  switch (type) {
    case ui::EventType::kGestureScrollBegin:
      // Synthetic creation of SCROLL_BEGIN from PINCH_BEGIN is explicitly
      // allowed as an exception.
      if (other.type() == ui::EventType::kGesturePinchBegin) {
        break;
      }
      [[fallthrough]];
    case ui::EventType::kGestureScrollUpdate:
    case ui::EventType::kScrollFlingStart:
    case ui::EventType::kGestureSwipe:
    case ui::EventType::kGesturePinchUpdate:
      DCHECK_EQ(type, other.type()) << " - Invalid gesture conversion from "
                                    << base::to_underlying(other.type())
                                    << " to " << base::to_underlying(type);
      break;
    default:
      break;
  }
}

GestureEventDetails::Details::Details() {
  memset(this, 0, sizeof(Details));
}

}  // namespace ui
