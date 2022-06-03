// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSORS_AURA_H_
#define UI_BASE_CURSOR_CURSORS_AURA_H_

#include "base/component_export.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace ui {
class Cursor;
enum class CursorSize;

// Returns data about |id|, where id is a cursor constant like
// ui::mojom::CursorType::kHelp. The IDR will be placed in |resource_id| and
// the hotspots for the different DPIs will be placed in |hot_1x| and
// |hot_2x|.  Returns false if |id| is invalid.
COMPONENT_EXPORT(UI_BASE_CURSOR)
bool GetCursorDataFor(CursorSize cursor_size,
                      mojom::CursorType id,
                      float scale_factor,
                      int* resource_id,
                      gfx::Point* point);

SkBitmap GetDefaultBitmap(const Cursor& cursor);

gfx::Point GetDefaultHotspot(const Cursor& cursor);

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSORS_AURA_H_
