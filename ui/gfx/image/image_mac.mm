// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/image/image_platform.h"

#import <AppKit/AppKit.h>
#include <stddef.h>

#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_internal.h"
#include "ui/gfx/image/image_png_rep.h"

namespace {

// Returns a 16x16 red NSImage to visually show when a NSImage cannot be
// created from PNG data.
NSImage* GetErrorNSImage() {
  return [NSImage imageWithSize:NSMakeSize(16, 16)
                        flipped:NO
                 drawingHandler:^(NSRect rect) {
                   [NSColor.redColor set];
                   NSRectFill(rect);
                   return YES;
                 }];
}

}  // namespace

namespace gfx {

namespace internal {

class ImageRepCocoa final : public ImageRep {
 public:
  explicit ImageRepCocoa(NSImage* image)
      : ImageRep(Image::kImageRepCocoa), image_(image) {
    CHECK(image_);
  }

  ImageRepCocoa(const ImageRepCocoa&) = delete;
  ImageRepCocoa& operator=(const ImageRepCocoa&) = delete;

  ~ImageRepCocoa() override = default;

  int Width() const override { return Size().width(); }

  int Height() const override { return Size().height(); }

  gfx::Size Size() const override {
    int width = static_cast<int>(image_.size.width);
    int height = static_cast<int>(image_.size.height);
    return gfx::Size(width, height);
  }

  NSImage* image() const { return image_; }

 private:
  NSImage* __strong image_;
};

const ImageRepCocoa* ImageRep::AsImageRepCocoa() const {
  CHECK_EQ(type_, Image::kImageRepCocoa);
  return reinterpret_cast<const ImageRepCocoa*>(this);
}
ImageRepCocoa* ImageRep::AsImageRepCocoa() {
  return const_cast<ImageRepCocoa*>(
      static_cast<const ImageRep*>(this)->AsImageRepCocoa());
}

scoped_refptr<base::RefCountedMemory> Get1xPNGBytesFromNSImage(
    NSImage* nsimage) {
  DCHECK(nsimage);

  // This function is to get the 1x bytes, so explicitly specify an identity
  // transform when extracting the pixels from this NSImage to get that 1x.
  NSDictionary<NSImageHintKey, id>* hints =
      @{NSImageHintCTM : [NSAffineTransform transform]};
  CGImageRef cg_image = [nsimage CGImageForProposedRect:nullptr
                                                context:nil
                                                  hints:hints];
  if (!cg_image) {
    // TODO(crbug.com/40805758): Look at DumpWithoutCrashing() reports to figure
    // out what's going on here.
    return scoped_refptr<base::RefCountedMemory>();
  }
  NSBitmapImageRep* ns_bitmap =
      [[NSBitmapImageRep alloc] initWithCGImage:cg_image];
  NSData* ns_data = [ns_bitmap representationUsingType:NSBitmapImageFileTypePNG
                                            properties:@{}];
  auto* bytes = static_cast<const uint8_t*>(ns_data.bytes);
  scoped_refptr<base::RefCountedBytes> refcounted_bytes(
      new base::RefCountedBytes());
  refcounted_bytes->as_vector().assign(bytes, bytes + ns_data.length);
  return refcounted_bytes;
}

NSImage* NSImageFromPNG(const std::vector<gfx::ImagePNGRep>& image_png_reps) {
  if (image_png_reps.empty()) {
    LOG(ERROR) << "Unable to decode PNG.";
    return GetErrorNSImage();
  }

  NSImage* image;
  for (const auto& image_png_rep : image_png_reps) {
    scoped_refptr<base::RefCountedMemory> png = image_png_rep.raw_data;
    CHECK(png.get());
    NSData* ns_data = [[NSData alloc] initWithBytes:png->front()
                                             length:png->size()];
    NSBitmapImageRep* ns_image_rep =
        [[NSBitmapImageRep alloc] initWithData:ns_data];
    if (!ns_image_rep) {
      LOG(ERROR) << "Unable to decode PNG at " << image_png_rep.scale << ".";
      return GetErrorNSImage();
    }
    if (!image) {
      float scale = image_png_rep.scale;
      NSSize image_size = NSMakeSize(ns_image_rep.pixelsWide / scale,
                                     ns_image_rep.pixelsHigh / scale);
      image = [[NSImage alloc] initWithSize:image_size];
    }
    [image addRepresentation:ns_image_rep];
  }

  return image;
}

NSImage* NSImageOfImageRepCocoa(const ImageRepCocoa* image_rep) {
  return image_rep->image();
}

std::unique_ptr<ImageRep> MakeImageRepCocoa(NSImage* image) {
  return std::make_unique<internal::ImageRepCocoa>(image);
}

}  // namespace internal

Image::Image(NSImage* image) {
  if (image) {
    storage_ = new internal::ImageStorage(Image::kImageRepCocoa);
    AddRepresentation(std::make_unique<internal::ImageRepCocoa>(image));
  }
}

}  // namespace gfx
