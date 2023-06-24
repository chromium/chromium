// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_BITMAP_CURSOR_H_
#define UI_OZONE_COMMON_BITMAP_CURSOR_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

// A cursor that is an SkBitmap combined with a gfx::Point hotspot.
class BitmapCursor : public PlatformCursor {
 public:
  static scoped_refptr<BitmapCursor> FromPlatformCursor(
      scoped_refptr<PlatformCursor> platform_cursor);

  // Creates a cursor that doesn't need backing bitmaps (for example, a
  // server-side cursor for Lacros). Scale only applies to bitmaps so
  // no need to provide scale. Server-side will provide both bitmap and
  // scale.
  explicit BitmapCursor(mojom::CursorType type);

  // Creates a cursor with a single backing bitmap.
  BitmapCursor(mojom::CursorType type,
               const SkBitmap& bitmap,
               const gfx::Point& hotspot,
               float cursor_image_scale_factor);

  // Creates a cursor with multiple bitmaps for animation.
  BitmapCursor(mojom::CursorType type,
               const std::vector<SkBitmap>& bitmaps,
               const gfx::Point& hotspot,
               base::TimeDelta frame_delay,
               float cursor_image_scale_factor);

  // Creates a cursor with external storage.
  BitmapCursor(mojom::CursorType type,
               void* platform_data,
               float cursor_image_scale_factor);

  BitmapCursor(const BitmapCursor&) = delete;
  BitmapCursor& operator=(const BitmapCursor&) = delete;

  mojom::CursorType type() const { return type_; }
  const gfx::Point& hotspot();
  const SkBitmap& bitmap();

  // For animated cursors.
  const std::vector<SkBitmap>& bitmaps();
  base::TimeDelta frame_delay();

  // For theme cursors.
  void* platform_data() { return platform_data_; }

  float cursor_image_scale_factor() const { return cursor_image_scale_factor_; }

 private:
  friend class base::RefCounted<PlatformCursor>;
  ~BitmapCursor() override;

  const mojom::CursorType type_;
  std::vector<SkBitmap> bitmaps_;
  gfx::Point hotspot_;
  base::TimeDelta frame_delay_;

  // Platform cursor data.  Having this non-nullptr means that this cursor
  // is supplied by the platform.
  const raw_ptr<void> platform_data_ = nullptr;

  float cursor_image_scale_factor_ = 1.f;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_BITMAP_CURSOR_H_
