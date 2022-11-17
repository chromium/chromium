// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/overlay_transform_utils.h"

#include "base/notreached.h"
#include "ui/gfx/geometry/rect_conversions.h"

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
      return Transform::Affine(-1, 0, 0, 1, viewport_bounds.width(), 0);
    case OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return Transform::Affine(1, 0, 0, -1, 0, viewport_bounds.height());
    case OVERLAY_TRANSFORM_ROTATE_90:
      return Transform::Affine(0, 1, -1, 0, viewport_bounds.height(), 0);
    case OVERLAY_TRANSFORM_ROTATE_180:
      return Transform::Affine(-1, 0, 0, -1, viewport_bounds.width(),
                               viewport_bounds.height());
    case OVERLAY_TRANSFORM_ROTATE_270:
      return Transform::Affine(0, -1, 1, 0, 0, viewport_bounds.width());
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
