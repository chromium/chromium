// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor_util.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/gfx/skia_util.h"

namespace ui {

namespace {

// Converts the SkBitmap to use a different alpha type. Returns true if bitmap
// was modified, otherwise returns false.
bool ConvertSkBitmapAlphaType(SkBitmap* bitmap, SkAlphaType alpha_type) {
  if (bitmap->info().alphaType() == alpha_type) {
    return false;
  }

  // Copy the bitmap into a temporary buffer. This will convert alpha type.
  SkImageInfo image_info =
      SkImageInfo::MakeN32(bitmap->width(), bitmap->height(), alpha_type);
  size_t info_row_bytes = image_info.minRowBytes();
  std::vector<char> buffer(image_info.computeByteSize(info_row_bytes));
  bitmap->readPixels(image_info, &buffer[0], info_row_bytes, 0, 0);
  // Read the temporary buffer back into the original bitmap.
  bitmap->reset();
  bitmap->allocPixels(image_info);
  // this memcpy call assumes bitmap->rowBytes() == info_row_bytes
  memcpy(bitmap->getPixels(), &buffer[0], buffer.size());

  return true;
}

}  // namespace

void ScaleAndRotateCursorBitmapAndHotpoint(float scale,
                                           display::Display::Rotation rotation,
                                           SkBitmap* bitmap,
                                           gfx::Point* hotpoint) {
  // SkBitmapOperations::Rotate() needs the bitmap to have premultiplied alpha,
  // so convert bitmap alpha type if we are going to rotate.
  bool was_converted = false;
  if (rotation != display::Display::ROTATE_0 &&
      bitmap->info().alphaType() == kUnpremul_SkAlphaType) {
    ConvertSkBitmapAlphaType(bitmap, kPremul_SkAlphaType);
    was_converted = true;
  }

  switch (rotation) {
    case display::Display::ROTATE_0:
      break;
    case display::Display::ROTATE_90:
      hotpoint->SetPoint(bitmap->height() - hotpoint->y(), hotpoint->x());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_90_CW);
      break;
    case display::Display::ROTATE_180:
      hotpoint->SetPoint(
          bitmap->width() - hotpoint->x(), bitmap->height() - hotpoint->y());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_180_CW);
      break;
    case display::Display::ROTATE_270:
      hotpoint->SetPoint(hotpoint->y(), bitmap->width() - hotpoint->x());
      *bitmap = SkBitmapOperations::Rotate(
          *bitmap, SkBitmapOperations::ROTATION_270_CW);
      break;
  }

  if (was_converted) {
    ConvertSkBitmapAlphaType(bitmap, kUnpremul_SkAlphaType);
  }

  if (scale < FLT_EPSILON) {
    NOTREACHED() << "Scale must be larger than 0.";
    scale = 1.0f;
  }

  if (scale == 1.0f)
    return;

  gfx::Size scaled_size = gfx::ScaleToFlooredSize(
      gfx::Size(bitmap->width(), bitmap->height()), scale);

  // TODO(crbug.com/919866): skia::ImageOperations::Resize() doesn't support
  // unpremultiplied alpha bitmaps.
  SkBitmap scaled_bitmap;
  scaled_bitmap.setInfo(
      bitmap->info().makeWH(scaled_size.width(), scaled_size.height()));
  if (scaled_bitmap.tryAllocPixels()) {
    bitmap->pixmap().scalePixels(scaled_bitmap.pixmap(),
                                 kMedium_SkFilterQuality);
  }

  *bitmap = scaled_bitmap;
  *hotpoint = gfx::ScaleToFlooredPoint(*hotpoint, scale);
}

void GetImageCursorBitmap(int resource_id,
                          float scale,
                          display::Display::Rotation rotation,
                          gfx::Point* hotspot,
                          SkBitmap* bitmap) {
  const gfx::ImageSkia* image =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  const gfx::ImageSkiaRep& image_rep = image->GetRepresentation(scale);
  // TODO(oshima): The cursor should use resource scale factor when
  // fractional scale factor is enabled. crbug.com/372212
  (*bitmap) = image_rep.GetBitmap();
  ScaleAndRotateCursorBitmapAndHotpoint(
      scale / image_rep.scale(), rotation, bitmap, hotspot);
  // |image_rep| is owned by the resource bundle. So we do not need to free it.
}

void GetAnimatedCursorBitmaps(int resource_id,
                              float scale,
                              display::Display::Rotation rotation,
                              gfx::Point* hotspot,
                              std::vector<SkBitmap>* bitmaps) {
  // TODO(oshima|tdanderson): Support rotation and fractional scale factor.
  const gfx::ImageSkia* image =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  const gfx::ImageSkiaRep& image_rep = image->GetRepresentation(scale);
  SkBitmap bitmap = image_rep.GetBitmap();
  int frame_width = bitmap.height();
  int frame_height = frame_width;
  int total_width = bitmap.width();
  DCHECK_EQ(total_width % frame_width, 0);
  int frame_count = total_width / frame_width;
  DCHECK_GT(frame_count, 0);

  bitmaps->resize(frame_count);

  for (int frame = 0; frame < frame_count; ++frame) {
    int x_offset = frame_width * frame;
    DCHECK_LE(x_offset + frame_width, total_width);

    SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(
        bitmap, x_offset, 0, frame_width, frame_height);
    DCHECK_EQ(frame_width, cropped.width());
    DCHECK_EQ(frame_height, cropped.height());

    (*bitmaps)[frame] = cropped;
  }
}

}  // namespace ui
