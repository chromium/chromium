// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include "base/notreached.h"
#include "ui/gfx/skia_util.h"

namespace ui {

Cursor::Cursor() = default;

Cursor::Cursor(mojom::CursorType type) : type_(type) {}

Cursor::Cursor(const Cursor& cursor) = default;

Cursor::~Cursor() = default;

void Cursor::SetPlatformCursor(scoped_refptr<PlatformCursor> platform_cursor) {
  platform_cursor_ = platform_cursor;
}

bool Cursor::operator==(const Cursor& cursor) const {
  return type_ == cursor.type_ && platform_cursor_ == cursor.platform_cursor_ &&
         image_scale_factor_ == cursor.image_scale_factor_ &&
         (type_ != mojom::CursorType::kCustom ||
          (custom_hotspot_ == cursor.custom_hotspot_ &&
           gfx::BitmapsAreEqual(custom_bitmap_, cursor.custom_bitmap_)));
}

}  // namespace ui
