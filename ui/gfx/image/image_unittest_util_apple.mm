// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_unittest_util.h"

#import <CoreGraphics/CoreGraphics.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/bit_cast.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#import <AppKit/AppKit.h>
#elif BUILDFLAG(IS_IOS)
#import <UIKit/UIKit.h>
#endif

namespace gfx::test {

// The |x| and |y| coordinates are interpreted as scale-independent on the Mac
// and on iOS.
SkColor GetPlatformImageColor(PlatformImage image, int x, int y) {
#if BUILDFLAG(IS_MAC)
  CGImageRef image_ref = [image CGImageForProposedRect:nil
                                               context:nil
                                                 hints:nil];
  CGRect target_pixel =
      CGRectMake(x * CGImageGetWidth(image_ref) / image.size.width,
                 y * CGImageGetHeight(image_ref) / image.size.height, 1, 1);
#elif BUILDFLAG(IS_IOS)
  CGImageRef image_ref = image.CGImage;
  CGRect target_pixel = CGRectMake(x * image.scale, y * image.scale, 1, 1);
#endif

  // Start by extracting the target pixel into a 1x1 CGImage.
  base::apple::ScopedCFTypeRef<CGImageRef> pixel_image(
      CGImageCreateWithImageInRect(image_ref, target_pixel));

  // Draw that pixel into a 1x1 bitmap context.
  base::apple::ScopedCFTypeRef<CGColorSpaceRef> color_space(
      CGColorSpaceCreateDeviceRGB());
  base::apple::ScopedCFTypeRef<CGContextRef> bitmap_context(
      CGBitmapContextCreate(
          /*data=*/nullptr,
          /*width=*/1,
          /*height=*/1,
          /*bitsPerComponent=*/8,
          /*bytesPerRow=*/4, color_space.get(),
          kCGImageAlphaPremultipliedFirst |
              static_cast<CGImageAlphaInfo>(kCGBitmapByteOrder32Host)));
  CGContextDrawImage(bitmap_context.get(), CGRectMake(0, 0, 1, 1),
                     pixel_image.get());

  // The CGBitmapContext has the same memory layout as SkColor, so we can just
  // read an SkColor straight out of the context.
  return *reinterpret_cast<SkColor*>(
      CGBitmapContextGetData(bitmap_context.get()));
}

}  // namespace gfx::test
