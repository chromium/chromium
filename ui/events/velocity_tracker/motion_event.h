// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_VELOCITY_TRACKER_MOTION_EVENT_H_
#define UI_EVENTS_VELOCITY_TRACKER_MOTION_EVENT_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/component_export.h"
#include "base/time/time.h"

namespace ui {

// Abstract class for a generic motion-related event, patterned after that
// subset of Android's MotionEvent API used in gesture detection.
class COMPONENT_EXPORT(VELOCITY_TRACKER) MotionEvent {
 public:
  enum class Action {
    NONE,
    DOWN,
    UP,
    MOVE,
    CANCEL,
    POINTER_DOWN,
    POINTER_UP,
    HOVER_ENTER,
    HOVER_EXIT,
    HOVER_MOVE,
    BUTTON_PRESS,
    BUTTON_RELEASE,
    LAST = BUTTON_RELEASE
  };

  enum class ToolType { UNKNOWN, FINGER, STYLUS, MOUSE, ERASER, LAST = ERASER };

  enum class Classification {
    NONE,
    AMBIGUOUS_GESTURE,
    DEEP_PRESS,
    LAST = DEEP_PRESS
  };

  enum ButtonType {
    BUTTON_PRIMARY = 1 << 0,
    BUTTON_SECONDARY = 1 << 1,
    BUTTON_TERTIARY = 1 << 2,
    BUTTON_BACK = 1 << 3,
    BUTTON_FORWARD = 1 << 4,
    BUTTON_STYLUS_PRIMARY = 1 << 5,
    BUTTON_STYLUS_SECONDARY = 1 << 6
  };

  // The implementer promises that |GetPointerId()| will never exceed
  // MAX_POINTER_ID.
  enum { MAX_POINTER_ID = 31, MAX_TOUCH_POINT_COUNT = 16 };

  virtual ~MotionEvent() {}

  // An unique identifier this motion event.
  virtual uint32_t GetUniqueEventId() const = 0;
  virtual Action GetAction() const = 0;
  // Only valid if |GetAction()| returns Action::POINTER_UP or
  // Action::POINTER_DOWN.
  virtual int GetActionIndex() const = 0;
  virtual size_t GetPointerCount() const = 0;
  virtual int GetPointerId(size_t pointer_index) const = 0;
  virtual float GetX(size_t pointer_index) const = 0;
  virtual float GetY(size_t pointer_index) const = 0;
  virtual float GetRawX(size_t pointer_index) const = 0;
  virtual float GetRawY(size_t pointer_index) const = 0;
  virtual float GetTouchMajor(size_t pointer_index) const = 0;
  virtual float GetTouchMinor(size_t pointer_index) const = 0;
  virtual float GetOrientation(size_t pointer_index) const = 0;
  virtual float GetPressure(size_t pointer_index) const = 0;
  virtual float GetTiltX(size_t pointer_index) const = 0;
  virtual float GetTiltY(size_t pointer_index) const = 0;
  virtual float GetTwist(size_t pointer_index) const = 0;
  virtual float GetTangentialPressure(size_t pointer_index) const = 0;
  virtual ToolType GetToolType(size_t pointer_index) const = 0;
  virtual int GetButtonState() const = 0;
  virtual int GetFlags() const = 0;
  virtual base::TimeTicks GetEventTime() const = 0;
  virtual base::TimeTicks GetLatestEventTime() const;

  virtual Classification GetClassification() const;

  // Optional historical data, default implementation provides an empty history.
  virtual size_t GetHistorySize() const;
  virtual base::TimeTicks GetHistoricalEventTime(size_t historical_index) const;
  virtual float GetHistoricalTouchMajor(size_t pointer_index,
                                        size_t historical_index) const;
  virtual float GetHistoricalX(size_t pointer_index,
                               size_t historical_index) const;
  virtual float GetHistoricalY(size_t pointer_index,
                               size_t historical_index) const;

  // Get the id of the device which created the event. Currently Aura only.
  virtual int GetSourceDeviceId(size_t pointer_index) const;

  // Utility accessor methods for convenience.
  int GetPointerId() const { return GetPointerId(0); }
  float GetX() const { return GetX(0); }
  float GetY() const { return GetY(0); }
  float GetRawX() const { return GetRawX(0); }
  float GetRawY() const { return GetRawY(0); }
  float GetRawOffsetX() const { return GetRawX() - GetX(); }
  float GetRawOffsetY() const { return GetRawY() - GetY(); }

  float GetTouchMajor() const { return GetTouchMajor(0); }
  float GetTouchMinor() const { return GetTouchMinor(0); }

  // Returns the orientation in radians. The meaning is overloaded:
  // * For a touch screen or pad, it's the orientation of the major axis
  //   clockwise from vertical. The return value lies in [-PI/2, PI/2].
  // * For a stylus, it indicates the direction in which the stylus is pointing.
  //   The return value lies in [-PI, PI].
  //   Stylus 3D orientation is returned in GetTiltX/Y. TODO(jkwang):
  //   Cleanup the stylus comment & usage here.
  float GetOrientation() const { return GetOrientation(0); }

  float GetPressure() const { return GetPressure(0); }
  // We have GetTiltX/Y here instead of GetTilt because MotionEvent spec is not
  // expressive enough for both 2D touch-surface geometry and 3D pen-orientation
  // geometry, as needed for PointerEvents:
  // https://w3c.github.io/pointerevents
  // Both GetTiltX and GetTiltY return angles in **degrees**, in the range
  // [-90,90]. See the PointerEvent spec link above for details
  float GetTiltX() const { return GetTiltX(0); }
  float GetTiltY() const { return GetTiltY(0); }
  float GetTwist() const { return GetTwist(0); }
  float GetTangentialPressure() const { return GetTangentialPressure(0); }

  ToolType GetToolType() const { return GetToolType(0); }

  // O(N) search of pointers (use sparingly!). Returns -1 if |id| nonexistent.
  int FindPointerIndexOfId(int id) const;

  // Note that these methods perform shallow copies of the originating events.
  // They guarantee only that the returned type will reflect the same
  // data exposed by the MotionEvent interface; no guarantees are made that the
  // underlying implementation is identical to the source implementation.
  std::unique_ptr<MotionEvent> Clone() const;
  std::unique_ptr<MotionEvent> Cancel() const;
};

COMPONENT_EXPORT(VELOCITY_TRACKER) std::ostream& operator<<(
    std::ostream& stream,
    const MotionEvent::Action action);
COMPONENT_EXPORT(VELOCITY_TRACKER) std::ostream& operator<<(
    std::ostream& stream,
    const MotionEvent::ToolType tool_type);

}  // namespace ui

#endif  // UI_EVENTS_VELOCITY_TRACKER_MOTION_EVENT_H_
