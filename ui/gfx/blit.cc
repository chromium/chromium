// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/blit.h"

#include <stddef.h>

#include "base/check.h"
#include "build/build_config.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {

namespace {

// Returns true if the given canvas has any part of itself clipped out or
// any non-identity tranform.
bool HasClipOrTransform(SkCanvas& canvas) {
  if (!canvas.getTotalMatrix().isIdentity())
    return true;

  if (!canvas.isClipRect())
    return true;

  // Now we know the clip is a regular rectangle, make sure it covers the
  // entire canvas.
  SkIRect clip_bounds = canvas.getDeviceClipBounds();

  SkImageInfo info;
  size_t row_bytes;
  void* pixels = canvas.accessTopLayerPixels(&info, &row_bytes);
  DCHECK(pixels);
  if (!pixels)
    return true;

  if (clip_bounds.fLeft != 0 || clip_bounds.fTop != 0 ||
      clip_bounds.fRight != info.width() ||
      clip_bounds.fBottom != info.height())
    return true;

  return false;
}

}  // namespace

void ScrollCanvas(SkCanvas* canvas,
                  const gfx::Rect& in_clip,
                  const gfx::Vector2d& offset) {
  DCHECK(!HasClipOrTransform(*canvas));  // Don't support special stuff.

  SkPixmap pixmap;
  bool success = skia::GetWritablePixels(canvas, &pixmap);
  DCHECK(success);

  // We expect all coords to be inside the canvas, so clip here.
  gfx::Rect clip = gfx::IntersectRects(
      in_clip, gfx::Rect(0, 0, pixmap.width(), pixmap.height()));

  // Compute the set of pixels we'll actually end up painting.
  gfx::Rect dest_rect = gfx::IntersectRects(clip + offset, clip);
  if (dest_rect.size().IsEmpty())
    return;  // Nothing to do.

  // Compute the source pixels that will map to the dest_rect
  gfx::Rect src_rect = dest_rect - offset;

  size_t row_bytes = dest_rect.width() * 4;
  if (offset.y() > 0) {
    // Data is moving down, copy from the bottom up.
    for (int y = dest_rect.height() - 1; y >= 0; y--) {
      memcpy(pixmap.writable_addr32(dest_rect.x(), dest_rect.y() + y),
             pixmap.addr32(src_rect.x(), src_rect.y() + y),
             row_bytes);
    }
  } else if (offset.y() < 0) {
    // Data is moving up, copy from the top down.
    for (int y = 0; y < dest_rect.height(); y++) {
      memcpy(pixmap.writable_addr32(dest_rect.x(), dest_rect.y() + y),
             pixmap.addr32(src_rect.x(), src_rect.y() + y),
             row_bytes);
    }
  } else if (offset.x() != 0) {
    // Horizontal-only scroll. We can do it in either top-to-bottom or bottom-
    // to-top, but have to be careful about the order for copying each row.
    // Fortunately, memmove already handles this for us.
    for (int y = 0; y < dest_rect.height(); y++) {
      memmove(pixmap.writable_addr32(dest_rect.x(), dest_rect.y() + y),
              pixmap.addr32(src_rect.x(), src_rect.y() + y),
              row_bytes);
    }
  }
}

}  // namespace gfx
