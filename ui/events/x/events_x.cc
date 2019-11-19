// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <stddef.h>
#include <string.h>
#include <cmath>

#include "base/time/time.h"
#include "build/build_config.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/events/devices/x11/device_list_cache_x11.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

EventType EventTypeFromNative(const PlatformEvent& native_event) {
  return EventTypeFromXEvent(*native_event);
}

int EventFlagsFromNative(const PlatformEvent& native_event) {
  return EventFlagsFromXEvent(*native_event);
}

base::TimeTicks EventTimeFromNative(const PlatformEvent& native_event) {
  base::TimeTicks timestamp = EventTimeFromXEvent(*native_event);
  ValidateEventTimeClock(&timestamp);
  return timestamp;
}

gfx::PointF EventLocationFromNative(const PlatformEvent& native_event) {
  return gfx::PointF(EventLocationFromXEvent(*native_event));
}

gfx::Point EventSystemLocationFromNative(const PlatformEvent& native_event) {
  return EventSystemLocationFromXEvent(*native_event);
}

int EventButtonFromNative(const PlatformEvent& native_event) {
  return EventButtonFromXEvent(*native_event);
}

KeyboardCode KeyboardCodeFromNative(const PlatformEvent& native_event) {
  return KeyboardCodeFromXKeyEvent(native_event);
}

DomCode CodeFromNative(const PlatformEvent& native_event) {
  return CodeFromXEvent(native_event);
}

bool IsCharFromNative(const PlatformEvent& native_event) {
  return false;
}

int GetChangedMouseButtonFlagsFromNative(const PlatformEvent& native_event) {
  return GetChangedMouseButtonFlagsFromXEvent(*native_event);
}

PointerDetails GetMousePointerDetailsFromNative(
    const PlatformEvent& native_event) {
  return PointerDetails(EventPointerType::POINTER_TYPE_MOUSE);
}

gfx::Vector2d GetMouseWheelOffset(const PlatformEvent& native_event) {
  return GetMouseWheelOffsetFromXEvent(*native_event);
}

PlatformEvent CopyNativeEvent(const PlatformEvent& native_event) {
  if (!native_event || native_event->type == GenericEvent)
    return NULL;
  XEvent* copy = new XEvent;
  *copy = *native_event;
  return copy;
}

void ReleaseCopiedNativeEvent(const PlatformEvent& native_event) {
  delete native_event;
}

int GetTouchId(const PlatformEvent& native_event) {
  return GetTouchIdFromXEvent(*native_event);
}

PointerDetails GetTouchPointerDetailsFromNative(
    const PlatformEvent& native_event) {
  return GetTouchPointerDetailsFromXEvent(*native_event);
}

bool GetScrollOffsets(const PlatformEvent& native_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count,
                      EventMomentumPhase* momentum_phase) {
  return GetScrollOffsetsFromXEvent(*native_event, x_offset, y_offset,
                                    x_offset_ordinal, y_offset_ordinal,
                                    finger_count);
}

bool GetFlingData(const PlatformEvent& native_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel) {
  return GetFlingDataFromXEvent(*native_event, vx, vy, vx_ordinal, vy_ordinal,
                                is_cancel);
}

}  // namespace ui
