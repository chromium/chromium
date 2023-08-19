// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_BITMAP_CURSOR_FACTORY_H_
#define UI_OZONE_COMMON_BITMAP_CURSOR_FACTORY_H_

#include <map>

#include "base/memory/scoped_refptr.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace ui {
class BitmapCursor;

// CursorFactory implementation for bitmapped cursors.
//
// This is a base class for platforms where PlatformCursor is an SkBitmap
// combined with a gfx::Point for the hotspot.
class BitmapCursorFactory : public CursorFactory {
 public:
  BitmapCursorFactory();
  BitmapCursorFactory(const BitmapCursorFactory&) = delete;
  BitmapCursorFactory& operator=(const BitmapCursorFactory&) = delete;
  ~BitmapCursorFactory() override;

  // CursorFactory:
  scoped_refptr<PlatformCursor> GetDefaultCursor(
      mojom::CursorType type) override;
  scoped_refptr<PlatformCursor> CreateImageCursor(mojom::CursorType type,
                                                  const SkBitmap& bitmap,
                                                  const gfx::Point& hotspot,
                                                  float scale) override;
  scoped_refptr<PlatformCursor> CreateAnimatedCursor(
      mojom::CursorType type,
      const std::vector<SkBitmap>& bitmaps,
      const gfx::Point& hotspot,
      float scale,
      base::TimeDelta frame_delay) override;

 private:
  std::map<mojom::CursorType, scoped_refptr<BitmapCursor>> default_cursors_;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_BITMAP_CURSOR_FACTORY_H_
