// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_

#include <stddef.h>

#include "base/time/time.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/types/event_type.h"
#include "ui/events/velocity_tracker/motion_event.h"

namespace ui {

class GestureEventDataPacket;

struct GESTURE_DETECTION_EXPORT GestureEventData {
  GestureEventData(const GestureEventDetails&,
                   int motion_event_id,
                   MotionEvent::ToolType primary_tool_type,
                   base::TimeTicks time,
                   float x,
                   float y,
                   float raw_x,
                   float raw_y,
                   size_t touch_point_count,
                   const gfx::RectF& bounding_box,
                   int flags,
                   uint32_t unique_touch_event_id);
  GestureEventData(EventType type, const GestureEventData&);
  GestureEventData(const GestureEventData& other);
  GestureEventData& operator=(const GestureEventData& other);

  EventType type() const { return details.type(); }

  GestureEventDetails details;
  int motion_event_id;
  // The tool type for the first touch point in the gesture.
  MotionEvent::ToolType primary_tool_type;
  base::TimeTicks time;
  float x;
  float y;
  float raw_x;
  float raw_y;
  int flags;

  // The unique id of the touch event that released the gesture event. This
  // field gets a non-zero from the corresponding field in
  // GestureEventDataPacket at the moment the gesture is pushed into the packet.
  uint32_t unique_touch_event_id;

 private:
  friend class GestureEventDataPacket;

  // Initializes type to GESTURE_TYPE_INVALID.
  GestureEventData();
};

}  //  namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_EVENT_DATA_H_
