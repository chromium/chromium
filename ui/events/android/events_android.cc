// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/input.h>

#include "base/notimplemented.h"
#include "base/time/time.h"
#include "ui/events/android/event_flags_android.h"
#include "ui/events/android/event_type_android.h"
#include "ui/events/android/key_event_android.h"
#include "ui/events/event_constants.h"
#include "ui/events/events_export.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_code_conversion_android.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/platform_event.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

EventType EventTypeFromNative(const PlatformEvent& native_event) {
  const KeyEventAndroid* key_event = native_event.AsKeyboardEventAndroid();
  if (key_event == nullptr) {
    return EventType::kUnknown;
  }
  return EventTypeFromAndroidKeyEventAction(key_event->Action());
}

int EventFlagsFromNative(const PlatformEvent& native_event) {
  const KeyEventAndroid* key_event = native_event.AsKeyboardEventAndroid();
  if (key_event == nullptr) {
    return 0;
  }
  return EventFlagsFromAndroidMetaState(key_event->MetaState());
}

base::TimeTicks EventTimeFromNative(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return base::TimeTicks();
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
  NOTIMPLEMENTED();
  return false;
}

// Exported for tests to use.
EVENTS_EXPORT PlatformEvent CreateInvalidPlatformEvent() {
  return PlatformEventAndroid();
}

bool IsPlatformEventValid(const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return false;
}

PointerDetails GetTouchPointerDetailsFromNative(
    const PlatformEvent& native_event) {
  NOTIMPLEMENTED();
  return PointerDetails(EventPointerType::kUnknown,
                        /* radius_x */ 1.0,
                        /* radius_y */ 1.0,
                        /* force */ 0.f,
                        /* twist */ 0.f,
                        /* tilt_x */ 0.f,
                        /* tilt_y */ 0.f);
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
  const KeyEventAndroid* key_event = native_event.AsKeyboardEventAndroid();
  if (key_event == nullptr) {
    return static_cast<KeyboardCode>(0);
  }
  return KeyboardCodeFromAndroidKeyCode(key_event->key_code());
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
