// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_utils.h"

#include <stddef.h>
#include <string.h>
#include <cmath>

#include "base/macros.h"
#include "base/memory/singleton.h"
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

namespace {

unsigned int UpdateX11EventFlags(int ui_flags, unsigned int old_x_flags) {
  static struct {
    int ui;
    int x;
  } flags[] = {
      {ui::EF_SHIFT_DOWN, ShiftMask},
      {ui::EF_CAPS_LOCK_ON, LockMask},
      {ui::EF_CONTROL_DOWN, ControlMask},
      {ui::EF_ALT_DOWN, Mod1Mask},
      {ui::EF_NUM_LOCK_ON, Mod2Mask},
      {ui::EF_MOD3_DOWN, Mod3Mask},
      {ui::EF_COMMAND_DOWN, Mod4Mask},
      {ui::EF_ALTGR_DOWN, Mod5Mask},
      {ui::EF_LEFT_MOUSE_BUTTON, Button1Mask},
      {ui::EF_MIDDLE_MOUSE_BUTTON, Button2Mask},
      {ui::EF_RIGHT_MOUSE_BUTTON, Button3Mask},
  };
  unsigned int new_x_flags = old_x_flags;
  for (size_t i = 0; i < arraysize(flags); ++i) {
    if (ui_flags & flags[i].ui)
      new_x_flags |= flags[i].x;
    else
      new_x_flags &= ~flags[i].x;
  }
  return new_x_flags;
}

unsigned int UpdateX11EventButton(int ui_flag, unsigned int old_x_button) {
  switch (ui_flag) {
    case ui::EF_LEFT_MOUSE_BUTTON:
      return Button1;
    case ui::EF_MIDDLE_MOUSE_BUTTON:
      return Button2;
    case ui::EF_RIGHT_MOUSE_BUTTON:
      return Button3;
    default:
      return old_x_button;
  }
  NOTREACHED();
}

}  // namespace

namespace ui {

EventType EventTypeFromNative(const PlatformEvent& native_event) {
  return EventTypeFromXEvent(*native_event);
}

int EventFlagsFromNative(const PlatformEvent& native_event) {
  return EventFlagsFromXEvent(*native_event);
}

base::TimeTicks EventTimeFromNative(const PlatformEvent& native_event) {
  return EventTimeFromXEvent(*native_event);
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

void ClearTouchIdIfReleased(const PlatformEvent& native_event) {
  ClearTouchIdIfReleasedFromXEvent(*native_event);
}

int GetTouchId(const PlatformEvent& native_event) {
  return GetTouchIdFromXEvent(*native_event);
}

PointerDetails GetTouchPointerDetailsFromNative(
    const PlatformEvent& native_event) {
  return PointerDetails(EventPointerType::POINTER_TYPE_TOUCH,
                        GetTouchIdFromXEvent(*native_event),
                        GetTouchRadiusXFromXEvent(*native_event),
                        GetTouchRadiusYFromXEvent(*native_event),
                        GetTouchForceFromXEvent(*native_event),
                        GetTouchAngleFromXEvent(*native_event));
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

void UpdateX11EventForFlags(Event* event) {
  XEvent* xev = event->native_event();
  if (!xev)
    return;
  switch (xev->type) {
    case KeyPress:
    case KeyRelease:
      xev->xkey.state = UpdateX11EventFlags(event->flags(), xev->xkey.state);
      break;
    case ButtonPress:
    case ButtonRelease:
      xev->xbutton.state =
          UpdateX11EventFlags(event->flags(), xev->xbutton.state);
      break;
    case GenericEvent: {
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
      DCHECK(xievent);
      xievent->mods.effective =
          UpdateX11EventFlags(event->flags(), xievent->mods.effective);
      break;
    }
    default:
      break;
  }
}

void UpdateX11EventForChangedButtonFlags(MouseEvent* event) {
  XEvent* xev = event->native_event();
  if (!xev)
    return;
  switch (xev->type) {
    case ButtonPress:
    case ButtonRelease:
      xev->xbutton.button = UpdateX11EventButton(event->changed_button_flags(),
                                                 xev->xbutton.button);
      break;
    case GenericEvent: {
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
      CHECK(xievent && (xievent->evtype == XI_ButtonPress ||
                        xievent->evtype == XI_ButtonRelease));
      xievent->detail =
          UpdateX11EventButton(event->changed_button_flags(), xievent->detail);
      break;
    }
    default:
      break;
  }
}

}  // namespace ui
