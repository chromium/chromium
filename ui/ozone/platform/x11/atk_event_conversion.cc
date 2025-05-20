// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/atk_event_conversion.h"

#include <cstdint>

#include "base/check.h"
#include "base/notreached.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/events/x/x11_event_translation.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_types.h"

namespace ui {

std::unique_ptr<AtkKeyEventStruct> AtkKeyEventFromXEvent(
    const x11::Event& event) {
  bool press;
  x11::KeyCode keycode;
  uint32_t state;
  x11::Time time;
  if (auto* xkey = event.As<x11::KeyEvent>()) {
    press = xkey->opcode == x11::KeyEvent::Press;
    keycode = xkey->detail;
    state = static_cast<uint32_t>(xkey->state);
    time = xkey->time;
  } else if (auto* xdevice = event.As<x11::Input::DeviceEvent>()) {
    if (xdevice->opcode != x11::Input::DeviceEvent::KeyPress &&
        xdevice->opcode != x11::Input::DeviceEvent::KeyRelease) {
      return nullptr;
    }
    press = xdevice->opcode == x11::Input::DeviceEvent::KeyPress;
    keycode = static_cast<x11::KeyCode>(xdevice->detail);
    state = XkbStateFromXI2Event(*xdevice);
    time = xdevice->time;
  } else {
    return nullptr;
  }

  auto atk_key_event = std::make_unique<AtkKeyEventStruct>();

  atk_key_event->type = press ? ATK_KEY_EVENT_PRESS : ATK_KEY_EVENT_RELEASE;
  atk_key_event->state = state;
  atk_key_event->keyval =
      x11::Connection::Get()->KeycodeToKeysym(keycode, state);
  atk_key_event->keycode = static_cast<guint16>(keycode);
  atk_key_event->timestamp = static_cast<guint32>(time);

  // This string property matches the one that was removed from GdkEventKey.
  // In the future, ATK clients should no longer rely on it, so we set it to
  // null.
  atk_key_event->string = nullptr;
  atk_key_event->length = 0;

  int flags = ui::GetEventFlagsFromXEvent(keycode, state, event.send_event());

  if (flags & ui::EF_SHIFT_DOWN) {
    atk_key_event->state |= AtkKeyModifierMask::kAtkShiftMask;
  }
  if (flags & ui::EF_CAPS_LOCK_ON) {
    atk_key_event->state |= AtkKeyModifierMask::kAtkLockMask;
  }
  if (flags & ui::EF_CONTROL_DOWN) {
    atk_key_event->state |= AtkKeyModifierMask::kAtkControlMask;
  }
  if (flags & ui::EF_ALT_DOWN) {
    atk_key_event->state |= AtkKeyModifierMask::kAtkMod1Mask;
  }
  if (flags & ui::EF_NUM_LOCK_ON) {
    atk_key_event->state |= AtkKeyModifierMask::kAtkMod2Mask;
  }
  if (flags & ui::EF_MOD3_DOWN) {
    atk_key_event->state |= AtkKeyModifierMask::kAtkMod3Mask;
  }
  if (flags & ui::EF_COMMAND_DOWN) {
    atk_key_event->state |= AtkKeyModifierMask::kAtkMod4Mask;
  }
  if (flags & ui::EF_ALTGR_DOWN) {
    atk_key_event->state |= AtkKeyModifierMask::KAtkMod5Mask;
  }

  return atk_key_event;
}

}  // namespace ui
