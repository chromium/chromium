// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_ATK_EVENT_CONVERSION_H_
#define UI_OZONE_PLATFORM_X11_ATK_EVENT_CONVERSION_H_

#include <atk/atk.h>

#include <memory>

#include "ui/gfx/x/event.h"

namespace ui {

// These values are duplicates of the GDK values that can be found in
// <gdk/gdktypes.h>. ATK expects the GDK values, but we don't want to depend on
// GDK here.
typedef enum {
  kAtkShiftMask = 1 << 0,
  kAtkLockMask = 1 << 1,
  kAtkControlMask = 1 << 2,
  kAtkMod1Mask = 1 << 3,
  kAtkMod2Mask = 1 << 4,
  kAtkMod3Mask = 1 << 5,
  kAtkMod4Mask = 1 << 6,
  KAtkMod5Mask = 1 << 7,
} AtkKeyModifierMask;

std::unique_ptr<AtkKeyEventStruct> AtkKeyEventFromXEvent(
    const x11::KeyEvent& xkey,
    bool send_event);

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_ATK_EVENT_CONVERSION_H_
