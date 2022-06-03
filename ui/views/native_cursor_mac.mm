// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/native_cursor.h"

#include <Cocoa/Cocoa.h>

#include "base/notreached.h"

namespace views {

gfx::NativeCursor GetNativeIBeamCursor() {
  return [NSCursor IBeamCursor];
}

gfx::NativeCursor GetNativeArrowCursor() {
  return [NSCursor arrowCursor];
}

gfx::NativeCursor GetNativeHandCursor() {
  return [NSCursor pointingHandCursor];
}

gfx::NativeCursor GetNativeColumnResizeCursor() {
  return [NSCursor resizeLeftRightCursor];
}

gfx::NativeCursor GetNativeEastWestResizeCursor() {
  NOTIMPLEMENTED();
  // TODO(tapted): This is the wrong cursor. Fetch the right one from WebCursor
  // or ResourceBundle or CoreCursor private API.
  return [NSCursor resizeLeftRightCursor];
}

gfx::NativeCursor GetNativeNorthSouthResizeCursor() {
  NOTIMPLEMENTED();
  return [NSCursor resizeUpDownCursor];
}

}  // namespace views
