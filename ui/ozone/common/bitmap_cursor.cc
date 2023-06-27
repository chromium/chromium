// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/bitmap_cursor.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"

namespace ui {

// static
scoped_refptr<BitmapCursor> BitmapCursor::FromPlatformCursor(
    scoped_refptr<PlatformCursor> platform_cursor) {
  return base::WrapRefCounted(
      static_cast<BitmapCursor*>(platform_cursor.get()));
}

BitmapCursor::BitmapCursor(mojom::CursorType type) : type_(type) {}

BitmapCursor::BitmapCursor(mojom::CursorType type,
                           const SkBitmap& bitmap,
                           const gfx::Point& hotspot,
                           float cursor_image_scale_factor)
    : type_(type),
      hotspot_(hotspot),
      cursor_image_scale_factor_(cursor_image_scale_factor) {
  if (!bitmap.isNull())
    bitmaps_.push_back(bitmap);
}

BitmapCursor::BitmapCursor(mojom::CursorType type,
                           const std::vector<SkBitmap>& bitmaps,
                           const gfx::Point& hotspot,
                           base::TimeDelta frame_delay,
                           float cursor_image_scale_factor)
    : type_(type),
      bitmaps_(bitmaps),
      hotspot_(hotspot),
      frame_delay_(frame_delay),
      cursor_image_scale_factor_(cursor_image_scale_factor) {
  DCHECK_LT(0U, bitmaps.size());
  DCHECK_LE(base::TimeDelta(), frame_delay);
  // No null bitmap should be in the list. Blank cursors should just be an empty
  // vector.
  DCHECK(base::ranges::none_of(bitmaps_, &SkBitmap::isNull));
}

BitmapCursor::BitmapCursor(mojom::CursorType type,
                           void* platform_data,
                           float cursor_image_scale_factor)
    : type_(type),
      platform_data_(platform_data),
      cursor_image_scale_factor_(cursor_image_scale_factor) {}

BitmapCursor::~BitmapCursor() = default;

const gfx::Point& BitmapCursor::hotspot() {
  return hotspot_;
}

const SkBitmap& BitmapCursor::bitmap() {
  return bitmaps_[0];
}

const std::vector<SkBitmap>& BitmapCursor::bitmaps() {
  return bitmaps_;
}

base::TimeDelta BitmapCursor::frame_delay() {
  return frame_delay_;
}

}  // namespace ui
