// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/mac_util.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"
#include "ui/gfx/canvas_paint_mac.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {

CanvasSkiaPaint::CanvasSkiaPaint(NSRect dirtyRect)
    : rectangle_(dirtyRect),
      composite_alpha_(false) {
  Init(true);
}

CanvasSkiaPaint::CanvasSkiaPaint(NSRect dirtyRect, bool opaque)
    : rectangle_(dirtyRect),
      composite_alpha_(false) {
  Init(opaque);
}

CanvasSkiaPaint::~CanvasSkiaPaint() {
  if (!is_empty()) {
    sk_canvas()->restoreToCount(1);

    // Blit the dirty rect to the current context.
    CGImageRef image = SkCreateCGImageRefWithColorspace(
        GetBitmap(), base::mac::GetSystemColorSpace());
    CGRect dest_rect = NSRectToCGRect(rectangle_);

    CGContextRef destination_context =
        (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    CGContextSaveGState(destination_context);
    CGContextSetBlendMode(
        destination_context,
        composite_alpha_ ? kCGBlendModeNormal : kCGBlendModeCopy);

    if ([[NSGraphicsContext currentContext] isFlipped]) {
      // Mirror context on the target's rect middle scanline.
      CGContextTranslateCTM(destination_context, 0.0, NSMidY(rectangle_));
      CGContextScaleCTM(destination_context, 1.0, -1.0);
      CGContextTranslateCTM(destination_context, 0.0, -NSMidY(rectangle_));
    }

    CGContextDrawImage(destination_context, dest_rect, image);
    CGContextRestoreGState(destination_context);

    CFRelease(image);
  }
}

void CanvasSkiaPaint::Init(bool opaque) {
  CGContextRef destination_context =
        (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
  CGRect scaled_unit_rect = CGContextConvertRectToDeviceSpace(
        destination_context, CGRectMake(0, 0, 1, 1));
    // Assume that the x scale and the y scale are the same.
  CGFloat scale = scaled_unit_rect.size.width;

  gfx::Size size(NSWidth(rectangle_), NSHeight(rectangle_));
  RecreateBackingCanvas(size, scale, opaque);
  cc::PaintCanvas* canvas = sk_canvas();
  canvas->clear(SK_ColorTRANSPARENT);

  // Need to translate so that the dirty region appears at the origin of the
  // surface.
  canvas->translate(-SkDoubleToScalar(NSMinX(rectangle_)),
                    -SkDoubleToScalar(NSMinY(rectangle_)));
}

}  // namespace skia


