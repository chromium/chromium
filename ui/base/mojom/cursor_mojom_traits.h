// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_MOJOM_CURSOR_MOJOM_TRAITS_H_
#define UI_BASE_MOJOM_CURSOR_MOJOM_TRAITS_H_

#include "ui/base/cursor/cursor.h"
#include "ui/base/mojom/cursor.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::CursorType, ui::CursorType> {
  static ui::mojom::CursorType ToMojom(ui::CursorType input);
  static bool FromMojom(ui::mojom::CursorType input, ui::CursorType* out);
};

template <>
struct EnumTraits<ui::mojom::CursorSize, ui::CursorSize> {
  static ui::mojom::CursorSize ToMojom(ui::CursorSize input);
  static bool FromMojom(ui::mojom::CursorSize input, ui::CursorSize* out);
};

template <>
struct StructTraits<ui::mojom::CursorDataView, ui::Cursor> {
  static ui::CursorType native_type(const ui::Cursor& c) {
    return c.native_type();
  }
  static gfx::Point hotspot(const ui::Cursor& c);
  static SkBitmap bitmap(const ui::Cursor& c);
  static float device_scale_factor(const ui::Cursor& c) {
    return c.device_scale_factor();
  }
  static bool Read(ui::mojom::CursorDataView data, ui::Cursor* out);
};

}  // namespace mojo

#endif  // UI_BASE_MOJOM_CURSOR_MOJOM_TRAITS_H_
