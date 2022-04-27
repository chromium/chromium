// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_NATIVE_CURSOR_H_
#define UI_VIEWS_NATIVE_CURSOR_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/views/views_export.h"

namespace views {

VIEWS_EXPORT gfx::NativeCursor GetNativeIBeamCursor();
VIEWS_EXPORT gfx::NativeCursor GetNativeHandCursor();
VIEWS_EXPORT gfx::NativeCursor GetNativeColumnResizeCursor();
VIEWS_EXPORT gfx::NativeCursor GetNativeEastWestResizeCursor();
VIEWS_EXPORT gfx::NativeCursor GetNativeNorthSouthResizeCursor();
VIEWS_EXPORT gfx::NativeCursor GetNativeNorthWestSouthEastResizeCursor();
VIEWS_EXPORT gfx::NativeCursor GetNativeNorthEastSouthWestResizeCursor();

}  // namespace views

#endif  // UI_VIEWS_NATIVE_CURSOR_H_
