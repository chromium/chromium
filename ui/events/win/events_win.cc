// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform_event.h"
#include "ui/events/win/events_win_utils.h"

namespace ui {

EventType EventTypeFromNative(const CHROME_MSG& native_event) {
  return EventTypeFromMSG(native_event);
}

int EventFlagsFromNative(const CHROME_MSG& native_event) {
  return EventFlagsFromMSG(native_event);
}

base::TimeTicks EventTimeFromNative(const CHROME_MSG& native_event) {
  // Note EventTimeFromMSG actually returns a time based on the current clock
  // tick, ignoring MSG. See the comments in that function (which is in
  // events_win_utils.cc) for the reason.
  return EventTimeFromMSG(native_event);
}

base::TimeTicks EventLatencyTimeFromNative(const CHROME_MSG& native_event,
                                           base::TimeTicks current_time) {
  // For latency calculations use the real timestamp, rather than the one
  // returned from EventTimeFromMSG.
  return EventLatencyTimeFromTickClock(native_event.time, current_time);
}

gfx::PointF EventLocationFromNative(const CHROME_MSG& native_event) {
  return gfx::PointF(EventLocationFromMSG(native_event));
}

gfx::Point EventSystemLocationFromNative(const CHROME_MSG& native_event) {
  return EventSystemLocationFromMSG(native_event);
}

KeyboardCode KeyboardCodeFromNative(const CHROME_MSG& native_event) {
  return KeyboardCodeFromMSG(native_event);
}

DomCode CodeFromNative(const CHROME_MSG& native_event) {
  return CodeFromMSG(native_event);
}

bool IsCharFromNative(const CHROME_MSG& native_event) {
  return IsCharFromMSG(native_event);
}

int GetChangedMouseButtonFlagsFromNative(const CHROME_MSG& native_event) {
  return GetChangedMouseButtonFlagsFromMSG(native_event);
}

PointerDetails GetMousePointerDetailsFromNative(
    const CHROME_MSG& native_event) {
  return GetMousePointerDetailsFromMSG(native_event);
}

gfx::Vector2d GetMouseWheelOffset(const CHROME_MSG& native_event) {
  return GetMouseWheelOffsetFromMSG(native_event);
}

gfx::Vector2d GetMouseWheelTick120ths(const CHROME_MSG& native_event) {
  // On Windows, the wheel offset is already in 120ths of a tick
  // (https://docs.microsoft.com/en-us/windows/win32/inputdev/wm-mousewheel).
  return GetMouseWheelOffsetFromMSG(native_event);
}

bool ShouldCopyPlatformEvents() {
  return true;
}

PlatformEvent CreateInvalidPlatformEvent() {
  CHROME_MSG msg = {0};
  return msg;
}

bool IsPlatformEventValid(const PlatformEvent& event) {
  return !(event.hwnd == 0 && event.message == 0 && event.wParam == 0 &&
           event.lParam == 0 && event.time == 0 && event.pt.x == 0 &&
           event.pt.y == 0);
}

PointerDetails GetTouchPointerDetailsFromNative(
    const CHROME_MSG& native_event) {
  NOTIMPLEMENTED();
  return PointerDetails(EventPointerType::kTouch,
                        /* pointer_id*/ 0,
                        /* radius_x */ 1.0,
                        /* radius_y */ 1.0,
                        /* force */ 0.f);
}

bool GetScrollOffsets(const CHROME_MSG& native_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count,
                      EventMomentumPhase* momentum_phase) {
  return GetScrollOffsetsFromMSG(native_event);
}

bool GetFlingData(const CHROME_MSG& native_event,
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
