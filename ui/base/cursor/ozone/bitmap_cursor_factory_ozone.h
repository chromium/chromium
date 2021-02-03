// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_OZONE_BITMAP_CURSOR_FACTORY_OZONE_H_
#define UI_BASE_CURSOR_OZONE_BITMAP_CURSOR_FACTORY_OZONE_H_

#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-forward.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

// A cursor that is an SkBitmap combined with a gfx::Point hotspot.
class COMPONENT_EXPORT(UI_BASE_CURSOR) BitmapCursorOzone
    : public base::RefCounted<BitmapCursorOzone> {
 public:
  // Creates a cursor that doesn't need backing bitmaps (for example, a
  // server-side cursor for Lacros).
  explicit BitmapCursorOzone(mojom::CursorType type);

  // Creates a cursor with a single backing bitmap.
  BitmapCursorOzone(mojom::CursorType type,
                    const SkBitmap& bitmap,
                    const gfx::Point& hotspot);

  // Creates a cursor with multiple bitmaps for animation.
  BitmapCursorOzone(mojom::CursorType type,
                    const std::vector<SkBitmap>& bitmaps,
                    const gfx::Point& hotspot,
                    int frame_delay_ms);

  // Creates a cursor with external storage.
  BitmapCursorOzone(mojom::CursorType type, void* platform_data);

  mojom::CursorType type() const { return type_; }
  const gfx::Point& hotspot();
  const SkBitmap& bitmap();

  // For animated cursors.
  const std::vector<SkBitmap>& bitmaps();
  int frame_delay_ms();

  // For theme cursors.
  void* platform_data() { return platform_data_; }

 private:
  friend class base::RefCounted<BitmapCursorOzone>;
  ~BitmapCursorOzone();

  const mojom::CursorType type_;
  std::vector<SkBitmap> bitmaps_;
  gfx::Point hotspot_;
  int frame_delay_ms_;

  // Platform cursor data.  Having this non-nullptr means that this cursor
  // is supplied by the platform.
  void* const platform_data_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(BitmapCursorOzone);
};

// CursorFactory implementation for bitmapped cursors.
//
// This is a base class for platforms where PlatformCursor is an SkBitmap
// combined with a gfx::Point for the hotspot.
class COMPONENT_EXPORT(UI_BASE_CURSOR) BitmapCursorFactoryOzone
    : public CursorFactory {
 public:
  BitmapCursorFactoryOzone();
  ~BitmapCursorFactoryOzone() override;

  // Convert PlatformCursor to BitmapCursorOzone.
  static scoped_refptr<BitmapCursorOzone> GetBitmapCursor(
      PlatformCursor platform_cursor);

  // CursorFactoryOzone:
  base::Optional<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type) override;
  PlatformCursor CreateImageCursor(mojom::CursorType type,
                                   const SkBitmap& bitmap,
                                   const gfx::Point& hotspot) override;
  PlatformCursor CreateAnimatedCursor(mojom::CursorType type,
                                      const std::vector<SkBitmap>& bitmaps,
                                      const gfx::Point& hotspot,
                                      int frame_delay_ms) override;
  void RefImageCursor(PlatformCursor cursor) override;
  void UnrefImageCursor(PlatformCursor cursor) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BitmapCursorFactoryOzone);
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_OZONE_BITMAP_CURSOR_FACTORY_OZONE_H_
