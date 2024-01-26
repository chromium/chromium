// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/common/bitmap_cursor_factory.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/ozone/common/bitmap_cursor.h"

namespace ui {

BitmapCursorFactory::BitmapCursorFactory() = default;

BitmapCursorFactory::~BitmapCursorFactory() = default;

scoped_refptr<PlatformCursor> BitmapCursorFactory::GetDefaultCursor(
    mojom::CursorType type) {
  if (!default_cursors_.count(type)) {
    // Return a cursor not backed by a bitmap to preserve the type information.
    // It can still be used to request the compositor to draw a server-side
    // cursor for the given type.
    // kNone is handled separately and does not need a bitmap.
    default_cursors_[type] = base::MakeRefCounted<BitmapCursor>(type);
  }

  return default_cursors_[type];
}

scoped_refptr<PlatformCursor> BitmapCursorFactory::CreateImageCursor(
    mojom::CursorType type,
    const SkBitmap& bitmap,
    const gfx::Point& hotspot,
    float scale) {
  return base::MakeRefCounted<BitmapCursor>(type, bitmap, hotspot, scale);
}

scoped_refptr<PlatformCursor> BitmapCursorFactory::CreateAnimatedCursor(
    mojom::CursorType type,
    const std::vector<SkBitmap>& bitmaps,
    const gfx::Point& hotspot,
    float scale,
    base::TimeDelta frame_delay) {
  DCHECK_LT(0U, bitmaps.size());
  return base::MakeRefCounted<BitmapCursor>(type, bitmaps, hotspot, frame_delay,
                                            scale);
}

}  // namespace ui
