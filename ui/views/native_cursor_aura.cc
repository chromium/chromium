// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/native_cursor.h"

#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace views {

gfx::NativeCursor GetNativeIBeamCursor() {
  return ui::mojom::CursorType::kIBeam;
}

gfx::NativeCursor GetNativeHandCursor() {
  return ui::mojom::CursorType::kHand;
}

gfx::NativeCursor GetNativeColumnResizeCursor() {
  return ui::mojom::CursorType::kColumnResize;
}

gfx::NativeCursor GetNativeEastWestResizeCursor() {
  return ui::mojom::CursorType::kEastWestResize;
}

gfx::NativeCursor GetNativeNorthSouthResizeCursor() {
  return ui::mojom::CursorType::kNorthSouthResize;
}

}  // namespace views
