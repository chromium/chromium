// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_POINTER_GRAB_H_
#define UI_BASE_X_X11_POINTER_GRAB_H_

#include "base/component_export.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

class X11Cursor;

// Grabs the pointer. It is unnecessary to ungrab the pointer prior to grabbing
// it.
COMPONENT_EXPORT(UI_BASE_X)
x11::GrabStatus GrabPointer(x11::Window window,
                            bool owner_events,
                            scoped_refptr<ui::X11Cursor> cursor);

// Sets the cursor to use for the duration of the active pointer grab.
COMPONENT_EXPORT(UI_BASE_X)
void ChangeActivePointerGrabCursor(scoped_refptr<ui::X11Cursor> cursor);

// Ungrabs the pointer.
COMPONENT_EXPORT(UI_BASE_X) void UngrabPointer();

}  // namespace ui

#endif  // UI_BASE_X_X11_POINTER_GRAB_H_
