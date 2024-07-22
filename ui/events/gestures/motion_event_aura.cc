// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/motion_event_aura.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace ui {
namespace {

MotionEvent::ToolType EventPointerTypeToMotionEventToolType(
    EventPointerType type) {
  switch (type) {
    case EventPointerType::kUnknown:
      return MotionEvent::ToolType::UNKNOWN;
    case EventPointerType::kMouse:
      return MotionEvent::ToolType::MOUSE;
    case EventPointerType::kPen:
      return MotionEvent::ToolType::STYLUS;
    case EventPointerType::kTouch:
      return MotionEvent::ToolType::FINGER;
    case EventPointerType::kEraser:
      return MotionEvent::ToolType::ERASER;
  }

  return MotionEvent::ToolType::UNKNOWN;
}

PointerProperties GetPointerPropertiesFromTouchEvent(const TouchEvent& touch) {
  PointerProperties pointer_properties;
  pointer_properties.x = touch.x();
  pointer_properties.y = touch.y();
  pointer_properties.raw_x = touch.root_location_f().x();
  pointer_properties.raw_y = touch.root_location_f().y();
  pointer_properties.id = touch.pointer_details().id;
  pointer_properties.pressure = touch.pointer_details().force;
  pointer_properties.source_device_id = touch.source_device_id();
  pointer_properties.tilt_x = touch.pointer_details().tilt_x;
  pointer_properties.tilt_y = touch.pointer_details().tilt_y;
  pointer_properties.twist = touch.pointer_details().twist;
  pointer_properties.tangential_pressure =
      touch.pointer_details().tangential_pressure;

  pointer_properties.SetAxesAndOrientation(touch.pointer_details().radius_x,
                                           touch.pointer_details().radius_y,
                                           touch.ComputeRotationAngle());
  if (!pointer_properties.touch_major) {
    float default_size;
    switch (touch.pointer_details().pointer_type) {
      case EventPointerType::kPen:
      case EventPointerType::kEraser:
        // Default size for stylus events is 1x1.
        default_size = 1;
        break;
      default:
        default_size =
            2.f * GestureConfiguration::GetInstance()->default_radius();
        break;
    }
    pointer_properties.touch_major = pointer_properties.touch_minor =
        default_size;
    pointer_properties.orientation = 0;
  }

  pointer_properties.tool_type = EventPointerTypeToMotionEventToolType(
      touch.pointer_details().pointer_type);

  return pointer_properties;
}

}  // namespace

MotionEventAura::MotionEventAura() {}

MotionEventAura::~MotionEventAura() {}

bool MotionEventAura::OnTouch(const TouchEvent& touch) {
  int index = FindPointerIndexOfId(touch.pointer_details().id);
  bool pointer_id_is_active = index != -1;

  if (touch.type() == EventType::kTouchPressed && pointer_id_is_active) {
    // TODO(tdresser): This should be NOTREACHED() - crbug.com/610423.
    return false;
  } else if (touch.type() != EventType::kTouchPressed &&
             !pointer_id_is_active) {
    // When a window begins capturing touch events, we could have an active
    // touch stream transfered to us, resulting in touch move or touch up events
    // without associated touch down events. Ignore them.
    return false;
  }

  if (touch.type() == EventType::kTouchMoved && touch.x() == GetX(index) &&
      touch.y() == GetY(index)) {
    return false;
  }

  switch (touch.type()) {
    case EventType::kTouchPressed:
      if (!AddTouch(touch))
        return false;
      [[fallthrough]];
    case EventType::kTouchReleased:
    case EventType::kTouchCancelled:
      // Removing these touch points needs to be postponed until after the
      // MotionEvent has been dispatched. This cleanup occurs in
      // CleanupRemovedTouchPoints.
      UpdateTouch(touch);
      break;
    case EventType::kTouchMoved:
      UpdateTouch(touch);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }

  UpdateCachedAction(touch);
  set_unique_event_id(touch.unique_event_id());
  set_flags(touch.flags());
  set_event_time(touch.time_stamp());
  return true;
}

void MotionEventAura::CleanupRemovedTouchPoints(const TouchEvent& event) {
  if (event.type() != EventType::kTouchReleased &&
      event.type() != EventType::kTouchCancelled) {
    return;
  }

  DCHECK(GetPointerCount());
  int index_to_delete = GetIndexFromId(event.pointer_details().id);
  set_action_index(-1);
  set_action(MotionEvent::Action::NONE);
  pointer(index_to_delete) = pointer(GetPointerCount() - 1);
  PopPointer();
}

int MotionEventAura::GetSourceDeviceId(size_t pointer_index) const {
  DCHECK_LT(pointer_index, GetPointerCount());
  return pointer(pointer_index).source_device_id;
}

bool MotionEventAura::AddTouch(const TouchEvent& touch) {
  if (GetPointerCount() == MotionEvent::MAX_TOUCH_POINT_COUNT)
    return false;

  PushPointer(GetPointerPropertiesFromTouchEvent(touch));
  return true;
}

void MotionEventAura::UpdateTouch(const TouchEvent& touch) {
  pointer(GetIndexFromId(touch.pointer_details().id)) =
      GetPointerPropertiesFromTouchEvent(touch);
}

void MotionEventAura::UpdateCachedAction(const TouchEvent& touch) {
  DCHECK(GetPointerCount());
  switch (touch.type()) {
    case EventType::kTouchPressed:
      if (GetPointerCount() == 1) {
        set_action(Action::DOWN);
      } else {
        set_action(Action::POINTER_DOWN);
        set_action_index(GetIndexFromId(touch.pointer_details().id));
      }
      break;
    case EventType::kTouchReleased:
      if (GetPointerCount() == 1) {
        set_action(Action::UP);
      } else {
        set_action(Action::POINTER_UP);
        set_action_index(GetIndexFromId(touch.pointer_details().id));
      }
      break;
    case EventType::kTouchCancelled:
      set_action(Action::CANCEL);
      break;
    case EventType::kTouchMoved:
      set_action(Action::MOVE);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

int MotionEventAura::GetIndexFromId(int id) const {
  int index = FindPointerIndexOfId(id);
  // TODO(tdresser): remove these checks once crbug.com/525189 is fixed.
  CHECK_GE(index, 0);
  CHECK_LT(index, static_cast<int>(GetPointerCount()));
  return index;
}

}  // namespace ui
