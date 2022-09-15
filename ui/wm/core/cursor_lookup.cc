// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_lookup.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"
#include "ui/wm/core/cursors_aura.h"

namespace wm {

SkBitmap GetCursorBitmap(const ui::Cursor& cursor) {
  if (cursor.type() == ui::mojom::CursorType::kCustom)
    return cursor.custom_bitmap();
  return GetDefaultBitmap(cursor);
}

gfx::Point GetCursorHotspot(const ui::Cursor& cursor) {
  if (cursor.type() == ui::mojom::CursorType::kCustom)
    return cursor.custom_hotspot();
  return GetDefaultHotspot(cursor);
}

}  // namespace wm
