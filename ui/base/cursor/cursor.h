// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_CURSOR_CURSOR_H_
#define UI_BASE_CURSOR_CURSOR_H_

#include <vector>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

struct COMPONENT_EXPORT(UI_BASE_CURSOR) CursorData {
 public:
  CursorData();
  CursorData(std::vector<SkBitmap> bitmaps, gfx::Point hotspot);
  CursorData(const CursorData&);
  ~CursorData();

  // `bitmaps` contains at least 1 element. Animated cursors (e.g.
  // `CursorType::kWait`, `CursorType::kProgress`) are represented as a list
  // of images, so a bigger number is expected.
  std::vector<SkBitmap> bitmaps;
  gfx::Point hotspot;
};

// Ref-counted cursor that supports both default and custom cursors.
class COMPONENT_EXPORT(UI_BASE_CURSOR) Cursor {
 public:
  // Creates a custom cursor with the provided parameters. `bitmap` dimensions
  // and `image_scale_factor` are DCHECKed to avoid integer overflow when
  // calculating the final cursor image size.
  static Cursor NewCustom(SkBitmap bitmap,
                          gfx::Point hotspot,
                          float image_scale_factor = 1.0f);
  Cursor();
  Cursor(mojom::CursorType type);
  Cursor(const Cursor& cursor);
  ~Cursor();

  void SetPlatformCursor(scoped_refptr<PlatformCursor> platform_cursor);

  mojom::CursorType type() const { return type_; }
  scoped_refptr<PlatformCursor> platform() const { return platform_cursor_; }
  float image_scale_factor() const { return image_scale_factor_; }
  void set_image_scale_factor(float scale) { image_scale_factor_ = scale; }

  const SkBitmap& custom_bitmap() const { return custom_bitmap_; }
  void set_custom_bitmap(const SkBitmap& bitmap) { custom_bitmap_ = bitmap; }

  const gfx::Point& custom_hotspot() const { return custom_hotspot_; }
  void set_custom_hotspot(const gfx::Point& hotspot) {
    custom_hotspot_ = hotspot;
  }

  // Note: custom cursor comparison may perform expensive pixel equality checks!
  bool operator==(const Cursor& cursor) const;
  bool operator!=(const Cursor& cursor) const { return !(*this == cursor); }

  bool operator==(mojom::CursorType type) const { return type_ == type; }
  bool operator!=(mojom::CursorType type) const { return type_ != type; }

 private:
  // Custom cursor constructor.
  Cursor(SkBitmap bitmap, gfx::Point hotspot, float image_scale_factor);

  mojom::CursorType type_ = mojom::CursorType::kNull;

  scoped_refptr<PlatformCursor> platform_cursor_;

  // Only used for custom cursors:
  SkBitmap custom_bitmap_;
  gfx::Point custom_hotspot_;

  // The scale factor for the cursor bitmap.
  float image_scale_factor_ = 1.0f;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_H_
