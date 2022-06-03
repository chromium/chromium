// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/overlay_transform_utils.h"

#include "base/notreached.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace gfx {

gfx::Transform OverlayTransformToTransform(
    gfx::OverlayTransform overlay_transform,
    const gfx::SizeF& viewport_bounds) {
  switch (overlay_transform) {
    case gfx::OVERLAY_TRANSFORM_INVALID:
      NOTREACHED();
      return gfx::Transform();
    case gfx::OVERLAY_TRANSFORM_NONE:
      return gfx::Transform();
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return gfx::Transform(
          SkMatrix::MakeAll(-1, 0, viewport_bounds.width(), 0, 1, 0, 0, 0, 1));
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return gfx::Transform(
          SkMatrix::MakeAll(1, 0, 0, 0, -1, viewport_bounds.height(), 0, 0, 1));
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return gfx::Transform(
          SkMatrix::MakeAll(0, -1, viewport_bounds.height(), 1, 0, 0, 0, 0, 1));
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return gfx::Transform(SkMatrix::MakeAll(-1, 0, viewport_bounds.width(), 0,
                                              -1, viewport_bounds.height(), 0,
                                              0, 1));
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return gfx::Transform(
          SkMatrix::MakeAll(0, 1, 0, -1, 0, viewport_bounds.width(), 0, 0, 1));
  }

  NOTREACHED();
  return gfx::Transform();
}

gfx::OverlayTransform InvertOverlayTransform(gfx::OverlayTransform transform) {
  switch (transform) {
    case gfx::OVERLAY_TRANSFORM_INVALID:
      NOTREACHED();
      return gfx::OVERLAY_TRANSFORM_NONE;
    case gfx::OVERLAY_TRANSFORM_NONE:
      return gfx::OVERLAY_TRANSFORM_NONE;
    case gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return gfx::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
    case gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return gfx::OVERLAY_TRANSFORM_FLIP_VERTICAL;
    case gfx::OVERLAY_TRANSFORM_ROTATE_90:
      return gfx::OVERLAY_TRANSFORM_ROTATE_270;
    case gfx::OVERLAY_TRANSFORM_ROTATE_180:
      return gfx::OVERLAY_TRANSFORM_ROTATE_180;
    case gfx::OVERLAY_TRANSFORM_ROTATE_270:
      return gfx::OVERLAY_TRANSFORM_ROTATE_90;
  }
  NOTREACHED();
  return gfx::OVERLAY_TRANSFORM_NONE;
}

}  // namespace gfx
