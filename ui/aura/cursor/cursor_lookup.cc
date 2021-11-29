// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/cursor/cursor_lookup.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/cursor/cursors_aura.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"

namespace aura {

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

}  // namespace aura
