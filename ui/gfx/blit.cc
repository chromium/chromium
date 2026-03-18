// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/blit.h"

#include <stddef.h>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/auto_spanification_helper.h"
#include "base/containers/span.h"
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

  const size_t pixels_per_row = static_cast<size_t>(dest_rect.width());
  auto copy_row = [pixels_per_row](SkPixmap& pixmap, const gfx::Rect& src_rect,
                                   const gfx::Rect& dest_rect, int y) {
    base::span<uint32_t> dest = UNSAFE_SKPIXMAP_GET_WRITABLE_ADDR32(
                                    pixmap, dest_rect.x(), dest_rect.y() + y)
                                    .first(pixels_per_row);
    base::span<const uint32_t> src =
        UNSAFE_SKPIXMAP_GET_ADDR32(pixmap, src_rect.x(), src_rect.y() + y)
            .first(pixels_per_row);
    dest.copy_from(src);
  };
  if (offset.y() > 0) {
    // Data is moving down, copy from the bottom up.
    for (int y = dest_rect.height() - 1; y >= 0; y--) {
      copy_row(pixmap, src_rect, dest_rect, y);
    }
  } else if (offset.y() < 0) {
    // Data is moving up, copy from the top down.
    for (int y = 0; y < dest_rect.height(); y++) {
      copy_row(pixmap, src_rect, dest_rect, y);
    }
  } else if (offset.x() != 0) {
    // Horizontal-only scroll. We can do it in either top-to-bottom or bottom-
    // to-top, but `copy_from()` already handles overlapping spans.
    for (int y = 0; y < dest_rect.height(); y++) {
      copy_row(pixmap, src_rect, dest_rect, y);
    }
  }
}

}  // namespace gfx
