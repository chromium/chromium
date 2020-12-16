// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_skia_rep_ios.h"

#include "ui/gfx/color_palette.h"

namespace gfx {

ImageSkiaRep::ImageSkiaRep() : scale_(0.0f) {}

ImageSkiaRep::ImageSkiaRep(const gfx::Size& size, float scale) : scale_(scale) {
  bitmap_.allocN32Pixels(static_cast<int>(size.width() * this->scale()),
                         static_cast<int>(size.height() * this->scale()));
  bitmap_.eraseColor(kPlaceholderColor);
  bitmap_.setImmutable();
  pixel_size_.SetSize(bitmap_.width(), bitmap_.height());
}

ImageSkiaRep::ImageSkiaRep(const SkBitmap& src, float scale)
    : pixel_size_(gfx::Size(src.width(), src.height())),
      bitmap_(src),
      scale_(scale) {
  bitmap_.setImmutable();
}

ImageSkiaRep::ImageSkiaRep(const ImageSkiaRep& other)
    : pixel_size_(other.pixel_size_),
      bitmap_(other.bitmap_),
      scale_(other.scale_) {}

ImageSkiaRep::~ImageSkiaRep() {}

int ImageSkiaRep::GetWidth() const {
  return static_cast<int>(pixel_width() / scale());
}

int ImageSkiaRep::GetHeight() const {
  return static_cast<int>(pixel_height() / scale());
}

const SkBitmap& ImageSkiaRep::GetBitmap() const {
  return bitmap_;
}

}  // namespace gfx
