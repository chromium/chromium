// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreGraphics/CoreGraphics.h>
#import <UIKit/UIKit.h>

#include "base/mac/scoped_cftyperef.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace gfx {
namespace test {

// The |x| and |y| coordinates are interpreted as scale-independent in ios.
SkColor GetPlatformImageColor(PlatformImage image, int x, int y) {
  // Start by extracting the target pixel into a 1x1 CGImage.
  base::ScopedCFTypeRef<CGImageRef> pixel_image(CGImageCreateWithImageInRect(
      image.CGImage, CGRectMake(x * image.scale, y * image.scale, 1, 1)));

  // Draw that pixel into a 1x1 bitmap context.
  base::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  base::ScopedCFTypeRef<CGContextRef> bitmap_context(CGBitmapContextCreate(
      /*data=*/NULL,
      /*width=*/1,
      /*height=*/1,
      /*bitsPerComponent=*/8,
      /*bytesPerRow=*/4, color_space,
      kCGImageAlphaPremultipliedFirst |
          static_cast<CGImageAlphaInfo>(kCGBitmapByteOrder32Host)));
  CGContextDrawImage(bitmap_context, CGRectMake(0, 0, 1, 1), pixel_image);

  // The CGBitmapContext has the same memory layout as SkColor, so we can just
  // read an SkColor straight out of the context.
  SkColor* data =
      reinterpret_cast<SkColor*>(CGBitmapContextGetData(bitmap_context));
  return *data;
}

}  // namespace test
}  // namespace gfx
