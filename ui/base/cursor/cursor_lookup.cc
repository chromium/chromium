// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_lookup.h"

#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursors_aura.h"
#endif

namespace ui {

SkBitmap GetCursorBitmap(const Cursor& cursor) {
  if (cursor.type() == mojom::CursorType::kCustom)
    return cursor.custom_bitmap();
#if defined(USE_AURA)
  return GetDefaultBitmap(cursor);
#else
  return SkBitmap();
#endif
}

gfx::Point GetCursorHotspot(const Cursor& cursor) {
  if (cursor.type() == mojom::CursorType::kCustom)
    return cursor.custom_hotspot();
#if defined(USE_AURA)
  return GetDefaultHotspot(cursor);
#else
  return gfx::Point();
#endif
}

}  // namespace ui
