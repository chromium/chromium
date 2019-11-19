// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"
#include "ui/events/win/events_win_utils.h"

namespace ui {

EventType EventTypeFromNative(const MSG& native_event) {
  return EventTypeFromMSG(native_event);
}

int EventFlagsFromNative(const MSG& native_event) {
  return EventFlagsFromMSG(native_event);
}

base::TimeTicks EventTimeFromNative(const MSG& native_event) {
  return EventTimeFromMSG(native_event);
}

gfx::PointF EventLocationFromNative(const MSG& native_event) {
  return gfx::PointF(EventLocationFromMSG(native_event));
}

gfx::Point EventSystemLocationFromNative(const MSG& native_event) {
  return EventSystemLocationFromMSG(native_event);
}

KeyboardCode KeyboardCodeFromNative(const MSG& native_event) {
  return KeyboardCodeFromMSG(native_event);
}

DomCode CodeFromNative(const MSG& native_event) {
  return CodeFromMSG(native_event);
}

bool IsCharFromNative(const MSG& native_event) {
  return IsCharFromMSG(native_event);
}

int GetChangedMouseButtonFlagsFromNative(const MSG& native_event) {
  return GetChangedMouseButtonFlagsFromMSG(native_event);
}

PointerDetails GetMousePointerDetailsFromNative(const MSG& native_event) {
  return GetMousePointerDetailsFromMSG(native_event);
}

gfx::Vector2d GetMouseWheelOffset(const MSG& native_event) {
  return GetMouseWheelOffsetFromMSG(native_event);
}

MSG CopyNativeEvent(const MSG& event) {
  return CopyMSGEvent(event);
}

void ReleaseCopiedNativeEvent(const MSG& event) {}

int GetTouchId(const MSG& xev) {
  NOTIMPLEMENTED();
  return 0;
}

PointerDetails GetTouchPointerDetailsFromNative(const MSG& native_event) {
  NOTIMPLEMENTED();
  return PointerDetails(EventPointerType::POINTER_TYPE_TOUCH,
                        /* pointer_id*/ 0,
                        /* radius_x */ 1.0,
                        /* radius_y */ 1.0,
                        /* force */ 0.f);
}

bool GetScrollOffsets(const MSG& native_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count,
                      EventMomentumPhase* momentum_phase) {
  return GetScrollOffsetsFromMSG(native_event);
}

bool GetFlingData(const MSG& native_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel) {
  // Not supported in Windows.
  NOTIMPLEMENTED();
  return false;
}

}  // namespace ui
