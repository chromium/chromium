// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_CURSOR_UTIL_H_
#define UI_WM_CORE_CURSOR_UTIL_H_

#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/display.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {
enum class CursorSize;
struct CursorData;
}  // namespace ui

namespace wm {

COMPONENT_EXPORT(UI_WM)
absl::optional<ui::CursorData> GetCursorData(
    ui::mojom::CursorType id,
    ui::CursorSize size,
    float scale,
    display::Display::Rotation rotation);

// Scale and rotate the cursor's bitmap and hotpoint.
// |bitmap_in_out| and |hotpoint_in_out| are used as
// both input and output.
COMPONENT_EXPORT(UI_WM)
void ScaleAndRotateCursorBitmapAndHotpoint(float scale,
                                           display::Display::Rotation rotation,
                                           SkBitmap* bitmap_in_out,
                                           gfx::Point* hotpoint_in_out);

}  // namespace wm

#endif  // UI_WM_CORE_CURSOR_UTIL_H_
