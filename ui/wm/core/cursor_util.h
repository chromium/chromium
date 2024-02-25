// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_CURSOR_UTIL_H_
#define UI_WM_CORE_CURSOR_UTIL_H_

#include <optional>

#include "base/component_export.h"
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

// Returns the cursor data corresponding to `type` and the rest of the
// parameters. If `target_cursor_size_in_px` presents, it will load the cursor
// resource and scale the bitmap and hotspot to match
// `target_cursor_size_in_px`, if not it will load the cursor resource and
// scale the bitmap and hotspot to match `scale`. The bitmap and hotspot are
// both in physical pixels.
COMPONENT_EXPORT(UI_WM)
std::optional<ui::CursorData> GetCursorData(
    ui::mojom::CursorType type,
    ui::CursorSize size,
    float scale,
    std::optional<int> target_cursor_size_in_px,
    display::Display::Rotation rotation);

// Scale and rotate the cursor's bitmap and hotpoint.
// |bitmap_in_out| and |hotpoint_in_out| are used as
// both input and output.
COMPONENT_EXPORT(UI_WM)
void ScaleAndRotateCursorBitmapAndHotpoint(float scale,
                                           display::Display::Rotation rotation,
                                           SkBitmap* bitmap_in_out,
                                           gfx::Point* hotpoint_in_out);

// Returns data about the cursor `type`. The IDR will be placed in `resource_id`
// and the hotspot in `point`. If `is_animated` is true it means the resource
// should be animated. Returns false if resource data for `type` isn't
// available.
COMPONENT_EXPORT(UI_WM)
bool GetCursorDataFor(ui::CursorSize cursor_size,
                      ui::mojom::CursorType type,
                      float scale_factor,
                      int* resource_id,
                      gfx::Point* point,
                      bool* is_animated);

}  // namespace wm

#endif  // UI_WM_CORE_CURSOR_UTIL_H_
