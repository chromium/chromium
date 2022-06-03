// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_LOOKUP_H_
#define UI_BASE_CURSOR_CURSOR_LOOKUP_H_

#include "base/component_export.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {
class Cursor;

COMPONENT_EXPORT(UI_BASE_CURSOR)
SkBitmap GetCursorBitmap(const Cursor& cursor);

COMPONENT_EXPORT(UI_BASE_CURSOR)
gfx::Point GetCursorHotspot(const Cursor& cursor);

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_LOOKUP_H_
