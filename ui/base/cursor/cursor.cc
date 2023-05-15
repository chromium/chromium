// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

namespace ui {

using mojom::CursorType;

CursorData::CursorData() : bitmaps({SkBitmap()}) {}

CursorData::CursorData(std::vector<SkBitmap> bitmaps,
                       gfx::Point hotspot,
                       float scale_factor)
    : bitmaps(std::move(bitmaps)),
      hotspot(std::move(hotspot)),
      scale_factor(scale_factor) {
  CHECK_GT(this->bitmaps.size(), 0u);
  CHECK_GT(scale_factor, 0);
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
      image_scale_factor_(image_scale_factor) {
  CHECK_GT(image_scale_factor_, 0);

  // Clamp the hotspot to the custom image's dimensions.
  if (!custom_bitmap_.empty()) {
    custom_hotspot_ =
        gfx::Point(std::clamp(hotspot.x(), 0, custom_bitmap_.width() - 1),
                   std::clamp(hotspot.y(), 0, custom_bitmap_.height() - 1));
  }
}

Cursor::Cursor(const Cursor& cursor) = default;

Cursor::~Cursor() = default;

void Cursor::SetPlatformCursor(scoped_refptr<PlatformCursor> platform_cursor) {
  platform_cursor_ = platform_cursor;
}

const SkBitmap& Cursor::custom_bitmap() const {
  CHECK_EQ(type_, CursorType::kCustom);
  return custom_bitmap_;
}

const gfx::Point& Cursor::custom_hotspot() const {
  CHECK_EQ(type_, CursorType::kCustom);
  return custom_hotspot_;
}

float Cursor::image_scale_factor() const {
  CHECK_EQ(type_, CursorType::kCustom);
  return image_scale_factor_;
}

bool Cursor::operator==(const Cursor& cursor) const {
  return type_ == cursor.type_ && platform_cursor_ == cursor.platform_cursor_ &&
         (type_ != CursorType::kCustom ||
          (image_scale_factor_ == cursor.image_scale_factor_ &&
           custom_hotspot_ == cursor.custom_hotspot_ &&
           gfx::BitmapsAreEqual(custom_bitmap_, cursor.custom_bitmap_)));
}

// static
bool Cursor::AreDimensionsValidForWeb(const gfx::Size& size,
                                      float scale_factor) {
  if (scale_factor == 0) {
    return false;
  }

  // https://developer.mozilla.org/en-US/docs/Web/CSS/cursor#icon_size_limits.
  static constexpr int kMaximumCursorDIPSize = 128;
  const gfx::Size size_in_dip = gfx::ScaleToCeiledSize(size, 1 / scale_factor);
  if (size_in_dip.width() > kMaximumCursorDIPSize ||
      size_in_dip.height() > kMaximumCursorDIPSize) {
    return false;
  }

  return true;
}

}  // namespace ui
