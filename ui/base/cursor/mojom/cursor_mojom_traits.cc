// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/mojom/cursor_mojom_traits.h"

#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace mojo {

// static
SkBitmap StructTraits<ui::mojom::CursorDataView, ui::Cursor>::bitmap(
    const ui::Cursor& c) {
  if (c.type() != ui::mojom::CursorType::kCustom) {
    return SkBitmap();
  }

  return c.custom_bitmap();
}

// static
gfx::Point StructTraits<ui::mojom::CursorDataView, ui::Cursor>::hotspot(
    const ui::Cursor& c) {
  if (c.type() != ui::mojom::CursorType::kCustom) {
    return gfx::Point();
  }

  return c.custom_hotspot();
}

// static
float StructTraits<ui::mojom::CursorDataView, ui::Cursor>::image_scale_factor(
    const ui::Cursor& c) {
  if (c.type() != ui::mojom::CursorType::kCustom) {
    return 1.0f;
  }

  return c.image_scale_factor();
}

// static
bool StructTraits<ui::mojom::CursorDataView, ui::Cursor>::Read(
    ui::mojom::CursorDataView data,
    ui::Cursor* out) {
  ui::mojom::CursorType type;
  if (!data.ReadType(&type)) {
    return false;
  }

  if (type != ui::mojom::CursorType::kCustom) {
    *out = ui::Cursor(type);
    return true;
  }

  gfx::Point hotspot;
  SkBitmap bitmap;
  if (!data.ReadHotspot(&hotspot) || !data.ReadBitmap(&bitmap)) {
    return false;
  }

  // Same check as in third_party/blink/renderer/core/input/event_handler.cc.
  // This will ensure that compromised renderers cannot bypass the cursor size
  // constraints, e.g. https://crbug.com/1246188.
  if (!ui::Cursor::AreDimensionsValidForWeb(
          gfx::SkISizeToSize(bitmap.dimensions()), data.image_scale_factor())) {
    return false;
  }

  *out = ui::Cursor::NewCustom(std::move(bitmap), std::move(hotspot),
                               data.image_scale_factor());
  return true;
}

}  // namespace mojo
