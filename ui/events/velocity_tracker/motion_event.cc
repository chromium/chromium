// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/velocity_tracker/motion_event.h"

#include <ostream>

#include "base/notreached.h"
#include "ui/events/velocity_tracker/motion_event_generic.h"

namespace ui {

size_t MotionEvent::GetHistorySize() const {
  return 0;
}

base::TimeTicks MotionEvent::GetHistoricalEventTime(
    size_t historical_index) const {
  NOTIMPLEMENTED();
  return base::TimeTicks();
}

base::TimeTicks MotionEvent::GetLatestEventTime() const {
  // Fallback to getting the event time which might be oldest event time in
  // presence of historical event times.
  return GetEventTime();
}

float MotionEvent::GetHistoricalTouchMajor(size_t pointer_index,
                                           size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0.f;
}

float MotionEvent::GetHistoricalX(size_t pointer_index,
                                  size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0.f;
}

float MotionEvent::GetHistoricalY(size_t pointer_index,
                                  size_t historical_index) const {
  NOTIMPLEMENTED();
  return 0.f;
}

MotionEvent::Classification MotionEvent::GetClassification() const {
  NOTIMPLEMENTED();
  return Classification::NONE;
}

int MotionEvent::FindPointerIndexOfId(int id) const {
  const size_t pointer_count = GetPointerCount();
  for (size_t i = 0; i < pointer_count; ++i) {
    if (GetPointerId(i) == id)
      return static_cast<int>(i);
  }
  return -1;
}

int MotionEvent::GetSourceDeviceId(size_t pointer_index) const {
  NOTIMPLEMENTED();
  return 0;
}

std::unique_ptr<MotionEvent> MotionEvent::Clone() const {
  return MotionEventGeneric::CloneEvent(*this);
}

std::unique_ptr<MotionEvent> MotionEvent::Cancel() const {
  return MotionEventGeneric::CancelEvent(*this);
}

std::ostream& operator<<(std::ostream& stream,
                         const MotionEvent::Action action) {
  return stream << static_cast<int>(action);
}
std::ostream& operator<<(std::ostream& stream,
                         const MotionEvent::ToolType tool_type) {
  return stream << static_cast<int>(tool_type);
}

}  // namespace ui
