// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#import <UIKit/UIKit.h>

#include "base/notreached.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ui {

EventType EventTypeFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return EventType::kUnknown;
}

int EventFlagsFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

base::TimeTicks EventTimeFromNative(const PlatformEvent& native_event) {
  base::TimeTicks timestamp =
      ui::EventTimeStampFromSeconds(native_event.Get().timestamp);
  ValidateEventTimeClock(&timestamp);
  return timestamp;
}

base::TimeTicks EventLatencyTimeFromNative(const PlatformEvent& native_event,
                                           base::TimeTicks current_time) {
  return EventTimeFromNative(native_event);
}

gfx::PointF EventLocationFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::PointF();
}

gfx::Point EventSystemLocationFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::Point();
}

int EventButtonFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

int GetChangedMouseButtonFlagsFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

PointerDetails GetMousePointerDetailsFromNative(
    const PlatformEvent& native_event) {
  return PointerDetails(EventPointerType::kMouse);
}

gfx::Vector2d GetMouseWheelOffset(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::Vector2d();
}

gfx::Vector2d GetMouseWheelTick120ths(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return gfx::Vector2d();
}

bool ShouldCopyPlatformEvents() {
  return true;
}

PlatformEvent CreateInvalidPlatformEvent() {
  return PlatformEvent();
}

bool IsPlatformEventValid(const PlatformEvent& event) {
  return !!event;
}

PointerDetails GetTouchPointerDetailsFromNative(
    const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return PointerDetails(EventPointerType::kUnknown,
                        /*pointer_id=*/0,
                        /*radius_x=*/1.0,
                        /*radius_y=*/1.0,
                        /*force=*/0.f);
}

bool GetScrollOffsets(const PlatformEvent& native_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count,
                      EventMomentumPhase* momentum_phase) {
  NOTIMPLEMENTED();
  return false;
}

bool GetFlingData(const PlatformEvent& native_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel) {
  NOTIMPLEMENTED();
  return false;
}

KeyboardCode KeyboardCodeFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return static_cast<KeyboardCode>(0);
}

DomCode CodeFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return DomCode::NONE;
}

bool IsCharFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return false;
}

uint32_t WindowsKeycodeFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

uint16_t TextFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

uint16_t UnmodifiedTextFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace ui
