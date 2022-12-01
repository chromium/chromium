// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_util.h"

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "ui/gfx/image/image.h"

namespace gfx {

bool JPEG1xEncodedDataFromImage(const Image& image,
                                int quality,
                                std::vector<unsigned char>* dst) {
  if (!image.HasRepresentation(gfx::Image::kImageRepCocoa))
    return JPEG1xEncodedDataFromSkiaRepresentation(image, quality, dst);

  NSImage* nsImage = image.ToNSImage();

  CGImageRef cgImage =
      [nsImage CGImageForProposedRect:nil context:nil hints:nil];
  base::scoped_nsobject<NSBitmapImageRep> rep(
      [[NSBitmapImageRep alloc] initWithCGImage:cgImage]);

  float compressionFactor = quality / 100.0;
  NSDictionary* options = @{ NSImageCompressionFactor : @(compressionFactor)};
  NSData* data = [rep representationUsingType:NSBitmapImageFileTypeJPEG
                                   properties:options];

  if ([data length] == 0)
    return false;

  dst->resize([data length]);
  [data getBytes:&dst->at(0) length:[data length]];
  return true;
}

}  // end namespace gfx
