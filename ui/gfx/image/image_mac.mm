// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_platform.h"

#import <AppKit/AppKit.h>
#include <stddef.h>

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_png_rep.h"

namespace gfx {
namespace internal {

namespace {

// Returns a 16x16 red NSImage to visually show when a NSImage cannot be
// created from PNG data.
// Caller takes ownership of the returned NSImage.
NSImage* GetErrorNSImage() {
  NSRect rect = NSMakeRect(0, 0, 16, 16);
  NSImage* image = [[NSImage alloc] initWithSize:rect.size];
  [image lockFocus];
  [[NSColor colorWithDeviceRed:1.0 green:0.0 blue:0.0 alpha:1.0] set];
  NSRectFill(rect);
  [image unlockFocus];
  return image;
}

}  // namespace

scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromNSImage(
    NSImage* nsimage) {
  DCHECK(nsimage);
  CGImageRef cg_image = [nsimage CGImageForProposedRect:NULL
                                                context:nil
                                                  hints:nil];
  if (!cg_image) {
    // TODO(crbug.com/1271762): Look at DumpWithoutCrashing() reports to figure
    // out what's going on here.
    return scoped_refptr<base::RefCountedMemory>();
  }
  base::scoped_nsobject<NSBitmapImageRep> ns_bitmap(
      [[NSBitmapImageRep alloc] initWithCGImage:cg_image]);
  NSData* ns_data = [ns_bitmap representationUsingType:NSPNGFileType
                                            properties:@{}];
  const unsigned char* bytes =
      static_cast<const unsigned char*>([ns_data bytes]);
  scoped_refptr<base::RefCountedBytes> refcounted_bytes(
      new base::RefCountedBytes());
  refcounted_bytes->data().assign(bytes, bytes + [ns_data length]);
  return refcounted_bytes;
}

NSImage* NSImageFromPNG(const std::vector<gfx::ImagePNGRep>& image_png_reps,
                        CGColorSpaceRef color_space) {
  if (image_png_reps.empty()) {
    LOG(ERROR) << "Unable to decode PNG.";
    return GetErrorNSImage();
  }

  base::scoped_nsobject<NSImage> image;
  for (size_t i = 0; i < image_png_reps.size(); ++i) {
    scoped_refptr<base::RefCountedMemory> png = image_png_reps[i].raw_data;
    CHECK(png.get());
    base::scoped_nsobject<NSData> ns_data(
        [[NSData alloc] initWithBytes:png->front() length:png->size()]);
    base::scoped_nsobject<NSBitmapImageRep> ns_image_rep(
        [[NSBitmapImageRep alloc] initWithData:ns_data]);
    if (!ns_image_rep) {
      LOG(ERROR) << "Unable to decode PNG at "
                 << image_png_reps[i].scale
                 << ".";
      return GetErrorNSImage();
    }

    // PNGCodec ignores colorspace related ancillary chunks (sRGB, iCCP). Ignore
    // colorspace information when decoding directly from PNG to an NSImage so
    // that the conversions: PNG -> SkBitmap -> NSImage and PNG -> NSImage
    // produce visually similar results.
    CGColorSpaceModel decoded_color_space_model = CGColorSpaceGetModel(
        [[ns_image_rep colorSpace] CGColorSpace]);
    CGColorSpaceModel color_space_model = CGColorSpaceGetModel(color_space);
    if (decoded_color_space_model == color_space_model) {
      base::scoped_nsobject<NSColorSpace> ns_color_space(
          [[NSColorSpace alloc] initWithCGColorSpace:color_space]);
      NSBitmapImageRep* ns_retagged_image_rep =
          [ns_image_rep
              bitmapImageRepByRetaggingWithColorSpace:ns_color_space];
      if (ns_retagged_image_rep && ns_retagged_image_rep != ns_image_rep)
        ns_image_rep.reset([ns_retagged_image_rep retain]);
    }

    if (!image.get()) {
      float scale = image_png_reps[i].scale;
      NSSize image_size = NSMakeSize([ns_image_rep pixelsWide] / scale,
                                     [ns_image_rep pixelsHigh] / scale);
      image.reset([[NSImage alloc] initWithSize:image_size]);
    }
    [image addRepresentation:ns_image_rep];
  }

  return image.autorelease();
}

gfx::Size NSImageSize(NSImage* image) {
  NSSize size = [image size];
  int width = static_cast<int>(size.width);
  int height = static_cast<int>(size.height);
  return gfx::Size(width, height);
}

} // namespace internal
} // namespace gfx

