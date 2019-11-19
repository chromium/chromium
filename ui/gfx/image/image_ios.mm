// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_platform.h"

#include <stddef.h>
#import <UIKit/UIKit.h>
#include <cmath>
#include <limits>

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_util_ios.h"

namespace gfx {
namespace internal {

namespace {

// Returns a 16x16 red UIImage to visually show when a UIImage cannot be
// created from PNG data. Logs error as well.
// Caller takes ownership of returned UIImage.
UIImage* CreateErrorUIImage(float scale) {
  LOG(ERROR) << "Unable to decode PNG into UIImage.";
  base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  base::ScopedCFTypeRef<CGContextRef> context(CGBitmapContextCreate(
      nullptr,  // Allow CG to allocate memory.
      16,       // width
      16,       // height
      8,        // bitsPerComponent
      0,        // CG will calculate by default.
      color_space, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
  CGContextSetRGBFillColor(context, 1.0, 0.0, 0.0, 1.0);
  CGContextFillRect(context, CGRectMake(0.0, 0.0, 16, 16));
  base::ScopedCFTypeRef<CGImageRef> cg_image(
      CGBitmapContextCreateImage(context));
  return [[UIImage imageWithCGImage:cg_image.get()
                              scale:scale
                        orientation:UIImageOrientationUp] retain];
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

scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromUIImage(
    UIImage* uiimage) {
  NSData* data = UIImagePNGRepresentation(uiimage);

  if ([data length] == 0)
    return nullptr;

  scoped_refptr<base::RefCountedBytes> png_bytes(
      new base::RefCountedBytes());
  png_bytes->data().resize([data length]);
  [data getBytes:&png_bytes->data().at(0) length:[data length]];
  return png_bytes;
}

UIImage* UIImageFromPNG(const std::vector<gfx::ImagePNGRep>& image_png_reps) {
  float ideal_scale = ImageSkia::GetMaxSupportedScale();

  if (image_png_reps.empty())
    return CreateErrorUIImage(ideal_scale);

  // Find best match for |ideal_scale|.
  float smallest_diff = std::numeric_limits<float>::max();
  size_t closest_index = 0u;
  for (size_t i = 0; i < image_png_reps.size(); ++i) {
    float scale = image_png_reps[i].scale;
    float diff = std::abs(ideal_scale - scale);
    if (diff < smallest_diff) {
      smallest_diff = diff;
      closest_index = i;
    }
  }

  return
      [CreateUIImageFromImagePNGRep(image_png_reps[closest_index]) autorelease];
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
  for (size_t i = 0; i < image_png_reps.size(); ++i) {
    base::scoped_nsobject<UIImage> uiimage(
        CreateUIImageFromImagePNGRep(image_png_reps[i]));
    gfx::ImageSkiaRep image_skia_rep = ImageSkiaRepOfScaleFromUIImage(
        uiimage, image_png_reps[i].scale);
    if (!image_skia_rep.is_null())
      image_skia.AddRepresentation(image_skia_rep);
  }
  return image_skia;
}

gfx::Size UIImageSize(UIImage* image) {
  int width = static_cast<int>(image.size.width);
  int height = static_cast<int>(image.size.height);
  return gfx::Size(width, height);
}

} // namespace internal
} // namespace gfx
