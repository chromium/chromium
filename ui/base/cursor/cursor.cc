// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/checked_math.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/skia_util.h"

namespace ui {

using mojom::CursorType;

CursorData::CursorData() : bitmaps({SkBitmap()}) {}

CursorData::CursorData(std::vector<SkBitmap> bitmaps, gfx::Point hotspot)
    : bitmaps(std::move(bitmaps)), hotspot(std::move(hotspot)) {
  DCHECK_GT(this->bitmaps.size(), 0u);
}

CursorData::CursorData(const CursorData& cursor_data) = default;

CursorData::~CursorData() = default;

// static
Cursor Cursor::NewCustom(SkBitmap bitmap,
                         gfx::Point hotspot,
                         float image_scale_factor) {
  return Cursor(std::move(bitmap), std::move(hotspot), image_scale_factor);
}

Cursor::Cursor() = default;

Cursor::Cursor(CursorType type) : type_(type) {}

Cursor::Cursor(SkBitmap bitmap, gfx::Point hotspot, float image_scale_factor)
    : type_(CursorType::kCustom),
      custom_bitmap_(std::move(bitmap)),
      custom_hotspot_(std::move(hotspot)),
      image_scale_factor_(image_scale_factor) {}

Cursor::Cursor(const Cursor& cursor) = default;

Cursor::~Cursor() = default;

void Cursor::SetPlatformCursor(scoped_refptr<PlatformCursor> platform_cursor) {
  platform_cursor_ = platform_cursor;
}

bool Cursor::operator==(const Cursor& cursor) const {
  return type_ == cursor.type_ && platform_cursor_ == cursor.platform_cursor_ &&
         image_scale_factor_ == cursor.image_scale_factor_ &&
         (type_ != CursorType::kCustom ||
          (custom_hotspot_ == cursor.custom_hotspot_ &&
           gfx::BitmapsAreEqual(custom_bitmap_, cursor.custom_bitmap_)));
}

}  // namespace ui
