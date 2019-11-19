// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/atk_event_conversion.h"

#include "ui/events/x/events_x_utils.h"

namespace ui {

std::unique_ptr<AtkKeyEventStruct> AtkKeyEventFromXEvent(XEvent* xevent) {
  DCHECK(xevent);
  auto atk_key_event = std::make_unique<AtkKeyEventStruct>();

  if (xevent->type == KeyPress)
    atk_key_event->type = ATK_KEY_EVENT_PRESS;
  else if (xevent->type == KeyRelease)
    atk_key_event->type = ATK_KEY_EVENT_RELEASE;
  else
    NOTREACHED() << xevent->type;

  XKeyEvent& xkey = xevent->xkey;
  KeySym keysym = NoSymbol;
  XLookupString(&xkey, nullptr, 0, &keysym, nullptr);

  atk_key_event->state = xkey.state;
  atk_key_event->keyval = keysym;
  atk_key_event->keycode = xkey.keycode;
  atk_key_event->timestamp = xkey.time;

  // This string property matches the one that was removed from GdkEventKey. In
  // the future, ATK clients should no longer rely on it, so we set it to null.
  atk_key_event->string = nullptr;
  atk_key_event->length = 0;

  int flags = ui::EventFlagsFromXEvent(*xevent);
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
