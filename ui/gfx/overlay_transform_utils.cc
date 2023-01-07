// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/overlay_transform_utils.h"

#include "base/notreached.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace gfx {

Transform OverlayTransformToTransform(OverlayTransform overlay_transform,
                                      const SizeF& viewport_bounds) {
  switch (overlay_transform) {
    case OVERLAY_TRANSFORM_INVALID:
      NOTREACHED();
      return Transform();
    case OVERLAY_TRANSFORM_NONE:
      return Transform();
    case OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return SkMatrixToTransform(
          SkMatrix::MakeAll(-1, 0, viewport_bounds.width(), 0, 1, 0, 0, 0, 1));
    case OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return SkMatrixToTransform(
          SkMatrix::MakeAll(1, 0, 0, 0, -1, viewport_bounds.height(), 0, 0, 1));
    case OVERLAY_TRANSFORM_ROTATE_90:
      return SkMatrixToTransform(
          SkMatrix::MakeAll(0, -1, viewport_bounds.height(), 1, 0, 0, 0, 0, 1));
    case OVERLAY_TRANSFORM_ROTATE_180:
      return SkMatrixToTransform(
          SkMatrix::MakeAll(-1, 0, viewport_bounds.width(), 0, -1,
                            viewport_bounds.height(), 0, 0, 1));
    case OVERLAY_TRANSFORM_ROTATE_270:
      return SkMatrixToTransform(
          SkMatrix::MakeAll(0, 1, 0, -1, 0, viewport_bounds.width(), 0, 0, 1));
  }

  NOTREACHED();
  return Transform();
}

OverlayTransform InvertOverlayTransform(OverlayTransform transform) {
  switch (transform) {
    case OVERLAY_TRANSFORM_INVALID:
      NOTREACHED();
      return OVERLAY_TRANSFORM_NONE;
    case OVERLAY_TRANSFORM_NONE:
      return OVERLAY_TRANSFORM_NONE;
    case OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
    case OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return OVERLAY_TRANSFORM_FLIP_VERTICAL;
    case OVERLAY_TRANSFORM_ROTATE_90:
      return OVERLAY_TRANSFORM_ROTATE_270;
    case OVERLAY_TRANSFORM_ROTATE_180:
      return OVERLAY_TRANSFORM_ROTATE_180;
    case OVERLAY_TRANSFORM_ROTATE_270:
      return OVERLAY_TRANSFORM_ROTATE_90;
  }
  NOTREACHED();
  return OVERLAY_TRANSFORM_NONE;
}

}  // namespace gfx
