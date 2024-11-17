// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/image/image_util.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "base/containers/to_vector.h"
#include "ui/gfx/image/image.h"

namespace gfx {

std::optional<std::vector<uint8_t>> JPEG1xEncodedDataFromImage(
    const Image& image,
    int quality) {
  if (!image.HasRepresentation(gfx::Image::kImageRepCocoa)) {
    return JPEG1xEncodedDataFromSkiaRepresentation(image, quality);
  }

  NSImage* nsImage = image.ToNSImage();
  CGImageRef cgImage = [nsImage CGImageForProposedRect:nil
                                               context:nil
                                                 hints:nil];
  NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];

  float compressionFactor = quality / 100.0;
  NSDictionary* options = @{NSImageCompressionFactor : @(compressionFactor)};
  NSData* data = [rep representationUsingType:NSBitmapImageFileTypeJPEG
                                   properties:options];

  if (data.length == 0) {
    return std::nullopt;
  }

  return base::ToVector(base::apple::NSDataToSpan(data));
}

}  // end namespace gfx
