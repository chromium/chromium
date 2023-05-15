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

namespace gfx {
class Size;
}

namespace ui {

struct COMPONENT_EXPORT(UI_BASE_CURSOR) CursorData {
 public:
  CursorData();
  CursorData(std::vector<SkBitmap> bitmaps,
             gfx::Point hotspot,
             float scale_factor = 1.0f);
  CursorData(const CursorData&);
  ~CursorData();

  // `bitmaps` contains at least 1 element. Animated cursors (e.g.
  // `CursorType::kWait`, `CursorType::kProgress`) are represented as a list
  // of images, so a bigger number is expected.
  std::vector<SkBitmap> bitmaps;
  gfx::Point hotspot;
  // `scale_factor` cannot be zero, since it will be either the device scale
  // factor or the image scale factor for custom cursors. In both cases, the
  // code is checked for a minimum value at its origin.
  float scale_factor = 1.0f;
};

// Ref-counted cursor that supports both default and custom cursors.
class COMPONENT_EXPORT(UI_BASE_CURSOR) Cursor {
 public:
  // Creates a custom cursor with the provided parameters. `hotspot` is
  // clamped to `bitmap` dimensions. `image_scale_factor` cannot be 0.
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

  // Methods to access custom cursor data. For any other cursor type, the
  // program will abort.
  const SkBitmap& custom_bitmap() const;
  const gfx::Point& custom_hotspot() const;
  float image_scale_factor() const;

  // Note: custom cursor comparison may perform expensive pixel equality checks!
  bool operator==(const Cursor& cursor) const;
  bool operator!=(const Cursor& cursor) const { return !(*this == cursor); }

  bool operator==(mojom::CursorType type) const { return type_ == type; }
  bool operator!=(mojom::CursorType type) const { return type_ != type; }

  // Limit the size of cursors so that they cannot be used to cover UI
  // elements in chrome.
  // `size` is the size of the cursor in physical pixels.
  static bool AreDimensionsValidForWeb(const gfx::Size& size,
                                       float scale_factor);

 private:
  // Custom cursor constructor.
  Cursor(SkBitmap bitmap, gfx::Point hotspot, float image_scale_factor);

  mojom::CursorType type_ = mojom::CursorType::kNull;

  scoped_refptr<PlatformCursor> platform_cursor_;

  // Only used for custom cursors:
  SkBitmap custom_bitmap_;
  gfx::Point custom_hotspot_;
  // Scale factor of `custom_bitmap_`. When creating the platform cursor, the
  // bitmap will be scaled to the device scale factor taking into account this
  // value.
  float image_scale_factor_ = 1.0f;
};

}  // namespace ui

#endif  // UI_BASE_CURSOR_CURSOR_H_
