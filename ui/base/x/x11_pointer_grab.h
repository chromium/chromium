// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_X11_POINTER_GRAB_H_
#define UI_BASE_X_X11_POINTER_GRAB_H_

#include "base/component_export.h"
#include "ui/gfx/x/x11_types.h"

typedef unsigned long Cursor;

namespace ui {

// Grabs the pointer. It is unnecessary to ungrab the pointer prior to grabbing
// it.
COMPONENT_EXPORT(UI_BASE_X)
int GrabPointer(XID window, bool owner_events, ::Cursor cursor);

// Sets the cursor to use for the duration of the active pointer grab.
COMPONENT_EXPORT(UI_BASE_X) void ChangeActivePointerGrabCursor(::Cursor cursor);

// Ungrabs the pointer.
COMPONENT_EXPORT(UI_BASE_X) void UngrabPointer();

}  // namespace ui

#endif  // UI_BASE_X_X11_POINTER_GRAB_H_
