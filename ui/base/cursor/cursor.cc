// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include <utility>

#include "base/notreached.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/skia_util.h"

namespace ui {

CursorData::CursorData() : bitmaps({SkBitmap()}) {}

CursorData::CursorData(std::vector<SkBitmap> bitmaps, gfx::Point hotspot)
    : bitmaps(std::move(bitmaps)), hotspot(std::move(hotspot)) {
  DCHECK_GT(this->bitmaps.size(), 0u);
}

CursorData::CursorData(const CursorData& cursor_data) = default;

CursorData::~CursorData() = default;

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
