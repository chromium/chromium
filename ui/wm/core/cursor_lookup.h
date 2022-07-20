// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_CURSOR_LOOKUP_H_
#define UI_WM_CORE_CURSOR_LOOKUP_H_

#include "base/component_export.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {
class Cursor;
}

namespace wm {

COMPONENT_EXPORT(UI_WM)
SkBitmap GetCursorBitmap(const ui::Cursor& cursor);

COMPONENT_EXPORT(UI_WM)
gfx::Point GetCursorHotspot(const ui::Cursor& cursor);

}  // namespace wm

#endif  // UI_WM_CORE_CURSOR_LOOKUP_H_
