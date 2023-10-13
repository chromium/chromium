// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/atk_event_conversion.h"

#include "base/check.h"
#include "base/notreached.h"
#include "ui/events/x/events_x_utils.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_types.h"

namespace ui {

std::unique_ptr<AtkKeyEventStruct> AtkKeyEventFromXEvent(
    const x11::KeyEvent& xkey,
    bool send_event) {
  auto atk_key_event = std::make_unique<AtkKeyEventStruct>();

  atk_key_event->type = xkey.opcode == x11::KeyEvent::Press
                            ? ATK_KEY_EVENT_PRESS
                            : ATK_KEY_EVENT_RELEASE;

  auto state = static_cast<int>(xkey.state);
  auto keycode = xkey.detail;
  auto keysym = x11::Connection::Get()->KeycodeToKeysym(keycode, state);

  atk_key_event->state = state;
  atk_key_event->keyval = keysym;
  atk_key_event->keycode = static_cast<uint8_t>(keycode);
  atk_key_event->timestamp = static_cast<uint32_t>(xkey.time);

  // This string property matches the one that was removed from GdkEventKey. In
  // the future, ATK clients should no longer rely on it, so we set it to null.
  atk_key_event->string = nullptr;
  atk_key_event->length = 0;

  int flags = ui::GetEventFlagsFromXKeyEvent(xkey, send_event);
  if (flags & ui::EF_SHIFT_DOWN)
    atk_key_event->state |= AtkKeyModifierMask::kAtkShiftMask;
  if (flags & ui::EF_CAPS_LOCK_ON)
    atk_key_event->state |= AtkKeyModifierMask::kAtkLockMask;
  if (flags & ui::EF_CONTROL_DOWN)
    atk_key_event->state |= AtkKeyModifierMask::kAtkControlMask;
  if (flags & ui::EF_ALT_DOWN)
    atk_key_event->state |= AtkKeyModifierMask::kAtkMod1Mask;
  if (flags & ui::EF_NUM_LOCK_ON)
    atk_key_event->state |= AtkKeyModifierMask::kAtkMod2Mask;
  if (flags & ui::EF_MOD3_DOWN)
    atk_key_event->state |= AtkKeyModifierMask::kAtkMod3Mask;
  if (flags & ui::EF_COMMAND_DOWN)
    atk_key_event->state |= AtkKeyModifierMask::kAtkMod4Mask;
  if (flags & ui::EF_ALTGR_DOWN)
    atk_key_event->state |= AtkKeyModifierMask::KAtkMod5Mask;

  return atk_key_event;
}

}  // namespace ui
