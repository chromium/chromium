// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_platform.h"

#include <stddef.h>
#import <UIKit/UIKit.h>

#include <cmath>
#include <limits>

#include "base/logging.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_internal.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_util_ios.h"

namespace {

// Returns a 16x16 red UIImage to visually show when a UIImage cannot be
// created from PNG data. Logs error as well.
UIImage* CreateErrorUIImage(float scale) {
  LOG(ERROR) << "Unable to decode PNG into UIImage.";
  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  base::apple::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      nullptr,  // Allow CG to allocate memory.
      16,       // width
      16,       // height
      8,        // bitsPerComponent
      0,        // CG will calculate by default.
      color_space.get(),
      kCGImageAlphaPremultipliedFirst |
          static_cast<CGImageAlphaInfo>(kCGBitmapByteOrder32Host)));
  CGContextSetRGBFillColor(context.get(), 1.0, 0.0, 0.0, 1.0);
  CGContextFillRect(context.get(), CGRectMake(0.0, 0.0, 16, 16));
  base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
      CGBitmapContextCreateImage(context.get()));
  return [UIImage imageWithCGImage:cg_image.get()
                             scale:scale
                       orientation:UIImageOrientationUp];
}

// Converts from ImagePNGRep to UIImage.
UIImage* CreateUIImageFromImagePNGRep(const gfx::ImagePNGRep& image_png_rep) {
  float scale = image_png_rep.scale;
  scoped_refptr<base::RefCountedMemory> png = image_png_rep.raw_data;
  CHECK(png.get());
  NSData* data = [NSData dataWithBytes:png->front() length:png->size()];
  UIImage* image = [[UIImage alloc] initWithData:data scale:scale];
  return image ? image : CreateErrorUIImage(scale);
}

}  // namespace

namespace gfx {

namespace internal {

class ImageRepCocoaTouch final : public ImageRep {
 public:
  explicit ImageRepCocoaTouch(UIImage* image)
      : ImageRep(Image::kImageRepCocoaTouch), image_(image) {
    CHECK(image_);
  }

  ImageRepCocoaTouch(const ImageRepCocoaTouch&) = delete;
  ImageRepCocoaTouch& operator=(const ImageRepCocoaTouch&) = delete;

  ~ImageRepCocoaTouch() override = default;

  int Width() const override { return Size().width(); }

  int Height() const override { return Size().height(); }

  gfx::Size Size() const override {
    int width = static_cast<int>(image_.size.width);
    int height = static_cast<int>(image_.size.height);
    return gfx::Size(width, height);
  }

  UIImage* image() const { return image_; }

 private:
  UIImage* __strong image_;
};

const ImageRepCocoaTouch* ImageRep::AsImageRepCocoaTouch() const {
  CHECK_EQ(type_, Image::kImageRepCocoaTouch);
  return reinterpret_cast<const ImageRepCocoaTouch*>(this);
}
ImageRepCocoaTouch* ImageRep::AsImageRepCocoaTouch() {
  return const_cast<ImageRepCocoaTouch*>(
      static_cast<const ImageRep*>(this)->AsImageRepCocoaTouch());
}

scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromUIImage(
    UIImage* uiimage) {
  DCHECK(uiimage);
  NSData* data = UIImagePNGRepresentation(uiimage);

  if (data.length == 0) {
    return nullptr;
  }

  scoped_refptr<base::RefCountedBytes> png_bytes(
      new base::RefCountedBytes());
  png_bytes->as_vector().resize(data.length);
  [data getBytes:&png_bytes->as_vector().at(0) length:data.length];
  return png_bytes;
}

UIImage* UIImageFromPNG(const std::vector<gfx::ImagePNGRep>& image_png_reps) {
  const float ideal_scale = ui::GetScaleForMaxSupportedResourceScaleFactor();

  if (image_png_reps.empty())
    return CreateErrorUIImage(ideal_scale);

  // Find best match for |ideal_scale|.
  float smallest_diff = std::numeric_limits<float>::max();
  size_t closest_index = 0u;
  for (size_t i = 0; i < image_png_reps.size(); ++i) {
    const float scale = image_png_reps[i].scale;
    const float diff = std::abs(ideal_scale - scale);
    if (diff < smallest_diff) {
      smallest_diff = diff;
      closest_index = i;
    }
  }

  return CreateUIImageFromImagePNGRep(image_png_reps[closest_index]);
}

scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromImageSkia(
    const ImageSkia* skia) {
  // iOS does not expose libpng, so conversion from ImageSkia to PNG must go
  // through UIImage.
  // TODO(rohitrao): Rewrite the callers of this function to save the UIImage
  // representation in the gfx::Image.  If we're generating it, we might as well
  // hold on to it.
  const gfx::ImageSkiaRep& image_skia_rep = skia->GetRepresentation(1.0f);
  if (image_skia_rep.scale() != 1.0f)
    return nullptr;

  UIImage* image = UIImageFromImageSkiaRep(image_skia_rep);
  return Get1xPNGBytesFromUIImage(image);
}

ImageSkia ImageSkiaFromPNG(
    const std::vector<gfx::ImagePNGRep>& image_png_reps) {
  // iOS does not expose libpng, so conversion from PNG to ImageSkia must go
  // through UIImage.
  ImageSkia image_skia;
  for (const auto& image_png_rep : image_png_reps) {
    UIImage* uiimage = CreateUIImageFromImagePNGRep(image_png_rep);
    gfx::ImageSkiaRep image_skia_rep =
        ImageSkiaRepOfScaleFromUIImage(uiimage, image_png_rep.scale);
    if (!image_skia_rep.is_null())
      image_skia.AddRepresentation(image_skia_rep);
  }
  return image_skia;
}

UIImage* UIImageOfImageRepCocoaTouch(const ImageRepCocoaTouch* image_rep) {
  return image_rep->image();
}

std::unique_ptr<ImageRep> MakeImageRepCocoaTouch(UIImage* image) {
  return std::make_unique<internal::ImageRepCocoaTouch>(image);
}

}  // namespace internal

Image::Image(UIImage* image) {
  if (image) {
    storage_ = new internal::ImageStorage(Image::kImageRepCocoaTouch);
    AddRepresentation(std::make_unique<internal::ImageRepCocoaTouch>(image));
  }
}

}  // namespace gfx
