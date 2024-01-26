// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/motion_event_test_utils.h"

#include <sstream>

#include "base/check_op.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/velocity_tracker/bitset_32.h"
#include "ui/events/velocity_tracker/motion_event.h"

using base::TimeTicks;

namespace ui {
namespace test {
namespace {

PointerProperties CreatePointer() {
  PointerProperties pointer;
  pointer.touch_major = MockMotionEvent::TOUCH_MAJOR;
  return pointer;
}

PointerProperties CreatePointer(float x, float y, int id) {
  PointerProperties pointer(x, y, MockMotionEvent::TOUCH_MAJOR);
  pointer.id = id;
  return pointer;
}

}  // namespace

MockMotionEvent::MockMotionEvent()
    : MotionEventGeneric(Action::CANCEL, base::TimeTicks(), CreatePointer()) {}

MockMotionEvent::MockMotionEvent(Action action)
    : MotionEventGeneric(action, base::TimeTicks(), CreatePointer()) {
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x0,
                                 float y0)
    : MotionEventGeneric(action, time, CreatePointer(x0, y0, 0)) {
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x0,
                                 float y0,
                                 float x1,
                                 float y1)
    : MotionEventGeneric(action, time, CreatePointer(x0, y0, 0)) {
  PushPointer(x1, y1);
  if (action == Action::POINTER_UP || action == Action::POINTER_DOWN)
    set_action_index(1);
}

MockMotionEvent::MockMotionEvent(Action action,
                                 TimeTicks time,
                                 float x0,
                                 float y0,
                                 float x1,
                                 float y1,
                                 float x2,
                                 float y2)
    : MotionEventGeneric(action, time, CreatePointer(x0, y0, 0)) {
  PushPointer(x1, y1);
  PushPointer(x2, y2);
  if (action == Action::POINTER_UP || action == Action::POINTER_DOWN)
    set_action_index(2);
}

MockMotionEvent::MockMotionEvent(Action action,
                                 base::TimeTicks time,
                                 const std::vector<gfx::PointF>& positions) {
  set_action(action);
  set_event_time(time);
  set_unique_event_id(ui::GetNextTouchEventId());
  if (action == Action::POINTER_UP || action == Action::POINTER_DOWN)
    set_action_index(static_cast<int>(positions.size()) - 1);
  for (size_t i = 0; i < positions.size(); ++i)
    PushPointer(positions[i].x(), positions[i].y());
}

MockMotionEvent::MockMotionEvent(const MockMotionEvent& other)
    : MotionEventGeneric(other) {
}

MockMotionEvent::~MockMotionEvent() {
}

MockMotionEvent& MockMotionEvent::PressPoint(float x, float y) {
  UpdatePointersAndID();
  PushPointer(x, y);
  if (GetPointerCount() > 1) {
    set_action_index(static_cast<int>(GetPointerCount()) - 1);
    set_action(Action::POINTER_DOWN);
  } else {
    set_action(Action::DOWN);
  }
  return *this;
}

MockMotionEvent& MockMotionEvent::MovePoint(size_t index, float x, float y) {
  UpdatePointersAndID();
  DCHECK_LT(index, GetPointerCount());
  PointerProperties& p = pointer(index);
  float dx = x - p.x;
  float dy = x - p.y;
  p.x = x;
  p.y = y;
  p.raw_x += dx;
  p.raw_y += dy;
  set_action(Action::MOVE);
  return *this;
}

MockMotionEvent& MockMotionEvent::ReleasePoint() {
  DCHECK_GT(GetPointerCount(), 0U);
  switch (GetAction()) {
    // If the previous action is one of those who need removing a pointer in
    // UpdatePointersAndID, then the last index will be GetPointerCount() - 2.
    case Action::POINTER_UP:
    case Action::UP:
    case Action::CANCEL:
      return ReleasePointAtIndex(GetPointerCount() - 2);
    default:
      break;
  }
  return ReleasePointAtIndex(GetPointerCount() - 1);
}

MockMotionEvent& MockMotionEvent::ReleasePointAtIndex(size_t index) {
  UpdatePointersAndID();
  DCHECK_LT(index, GetPointerCount());
  if (GetPointerCount() > 1) {
    set_action_index(static_cast<int>(index));
    set_action(Action::POINTER_UP);
  } else {
    set_action(Action::UP);
  }
  return *this;
}

MockMotionEvent& MockMotionEvent::CancelPoint() {
  UpdatePointersAndID();
  DCHECK_GT(GetPointerCount(), 0U);
  set_action(Action::CANCEL);
  return *this;
}

MockMotionEvent& MockMotionEvent::SetTouchMajor(float new_touch_major) {
  for (size_t i = 0; i < GetPointerCount(); ++i)
    pointer(i).touch_major = new_touch_major;
  return *this;
}

MockMotionEvent& MockMotionEvent::SetRawOffset(float raw_offset_x,
                                               float raw_offset_y) {
  for (size_t i = 0; i < GetPointerCount(); ++i) {
    pointer(i).raw_x = pointer(i).x + raw_offset_x;
    pointer(i).raw_y = pointer(i).y + raw_offset_y;
  }
  return *this;
}

MockMotionEvent& MockMotionEvent::SetToolType(size_t pointer_index,
                                              ToolType tool_type) {
  DCHECK_LT(pointer_index, GetPointerCount());
  pointer(pointer_index).tool_type = tool_type;
  return *this;
}

void MockMotionEvent::PushPointer(float x, float y) {
  MotionEventGeneric::PushPointer(
      CreatePointer(x, y, static_cast<int>(GetPointerCount())));
}

void MockMotionEvent::UpdatePointersAndID() {
  set_unique_event_id(ui::GetNextTouchEventId());
  switch (GetAction()) {
    case Action::POINTER_UP: {
      int index = GetActionIndex();
      DCHECK_LT(index, static_cast<int>(GetPointerCount()));
      RemovePointerAt(index);
      break;
    }
    case Action::UP:
    case Action::CANCEL:
      PopPointer();
      break;
    default:
      break;
  }
  set_action_index(-1);
}

MockMotionEvent& MockMotionEvent::SetPrimaryPointerId(int id) {
  DCHECK_GT(GetPointerCount(), 0U);
  pointer(0).id = id;
  return *this;
}

MotionEvent::Classification MockMotionEvent::GetClassification() const {
  return gesture_classification_;
}

std::string ToString(const MotionEvent& event) {
  std::stringstream ss;
  ss << "MotionEvent {"
     << "\n Action: " << event.GetAction();
  if (event.GetAction() == MotionEvent::Action::POINTER_DOWN ||
      event.GetAction() == MotionEvent::Action::POINTER_UP)
    ss << "\n ActionIndex: " << event.GetActionIndex();
  ss << "\n Flags: " << event.GetFlags()
     << "\n ButtonState: " << event.GetButtonState() << "\n Pointers: [";
  const size_t pointer_count = event.GetPointerCount();
  const size_t history_size = event.GetHistorySize();

  BitSet32 pointer_ids;
  for (size_t i = 0; i < pointer_count; ++i) {
    pointer_ids.mark_bit(event.GetPointerId(i));

    // Print the pointers sorted by id.
    while (!pointer_ids.is_empty()) {
      int pi = event.FindPointerIndexOfId(pointer_ids.first_marked_bit());
      DCHECK_GE(pi, 0);
      pointer_ids.clear_first_marked_bit();
      ss << "{"
         << "\n  PointerId: (" << event.GetPointerId(pi) << ")"
         << "\n  Pos: (" << event.GetX(pi) << ", " << event.GetY(pi) << ")"
         << "\n  RawPos: (" << event.GetX(pi) << ", " << event.GetY(pi) << ")"
         << "\n  Size: (" << event.GetTouchMajor(pi) << ", "
         << event.GetTouchMinor(pi) << ")"
         << "\n  Orientation: " << event.GetOrientation(pi)
         << "\n  Pressure: " << event.GetPressure(pi)
         << "\n  TiltX: " << event.GetTiltX(pi)
         << "\n  TiltY: " << event.GetTiltY(pi)
         << "\n  Tool: " << event.GetToolType(pi);
      if (history_size) {
        ss << "\n  History: [";
        for (size_t h = 0; h < history_size; ++h) {
          ss << "\n   { " << event.GetHistoricalX(pi, h) << ", "
             << event.GetHistoricalY(pi, h) << ", "
             << event.GetHistoricalTouchMajor(pi, h) << ", "
             << event.GetHistoricalEventTime(pi) << " }";
          if (h + 1 < history_size)
            ss << ",";
        }
        ss << "\n  ]";
      }
      ss << "\n }";
      if (i + 1 < pointer_count)
        ss << ", ";
    }
    ss << "]\n}";
  }

  return ss.str();
}

}  // namespace test
}  // namespace ui
