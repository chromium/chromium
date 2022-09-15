// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IMAGE_IMAGE_SKIA_REP_IOS_H_
#define UI_GFX_IMAGE_IMAGE_SKIA_REP_IOS_H_

#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// An ImageSkiaRep represents an image and the scale factor it is intended for.
// 0.0f scale is used to indicate that this ImageSkiaRep is used for unscaled
// image (ImageSkia does not automatically scale the image).
// iOS does not support cc's PaintOpBuffer and instead uses cocoa frameworks
// image formats.
class GFX_EXPORT ImageSkiaRep {
 public:
  // Create null bitmap.
  ImageSkiaRep();

  // Note: This is for testing purpose only.
  // Creates a bitmap with kARGB_8888_Config config with given |size| in DIP.
  // This allocates pixels in the bitmap. It is guaranteed that the data in the
  // bitmap are initialized but the actual values are undefined.
  // Specifying 0 scale means the image is for unscaled image. (unscaled()
  // returns truen, and scale() returns 1.0f;)
  ImageSkiaRep(const gfx::Size& size, float scale);

  // Creates a bitmap with given scale.
  // Adds ref to |src|.
  ImageSkiaRep(const SkBitmap& src, float scale);
  ImageSkiaRep(const ImageSkiaRep& other);

  ~ImageSkiaRep();

  // Get width and height of the image in pixels.
  int pixel_width() const { return bitmap_.width(); }
  int pixel_height() const { return bitmap_.height(); }
  Size pixel_size() const { return gfx::Size(pixel_width(), pixel_height()); }

  // Get width and height of the image in DIP.
  int GetWidth() const;
  int GetHeight() const;

  // Retrieves the scale for which this image is a representation of.
  float scale() const { return unscaled() ? 1.0f : scale_; }
  bool unscaled() const { return scale_ == 0.0f; }

  bool is_null() const { return bitmap_.isNull(); }

  // Returns the backing bitmap when the image representation is sourced from a
  // bitmap.
  const SkBitmap& GetBitmap() const;

 private:
  Size pixel_size_;
  SkBitmap bitmap_;
  float scale_;
};

}  // namespace gfx

#endif  // UI_GFX_IMAGE_IMAGE_SKIA_REP_IOS_H_
