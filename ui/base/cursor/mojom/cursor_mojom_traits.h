// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_MOJOM_CURSOR_MOJOM_TRAITS_H_
#define UI_BASE_CURSOR_MOJOM_CURSOR_MOJOM_TRAITS_H_

#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor.mojom-shared.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

class SkBitmap;

namespace gfx {
class Point;
}

namespace mojo {

template <>
struct StructTraits<ui::mojom::CursorDataView, ui::Cursor> {
  static ui::mojom::CursorType type(const ui::Cursor& c) { return c.type(); }
  static SkBitmap bitmap(const ui::Cursor& c);
  static gfx::Point hotspot(const ui::Cursor& c);
  static float image_scale_factor(const ui::Cursor& c);
  static bool Read(ui::mojom::CursorDataView data, ui::Cursor* out);
};

}  // namespace mojo

#endif  // UI_BASE_CURSOR_MOJOM_CURSOR_MOJOM_TRAITS_H_
