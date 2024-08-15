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
      NOTREACHED_IN_MIGRATION();
      return Transform();
    case OVERLAY_TRANSFORM_NONE:
      return Transform();
    case OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return Transform::Affine(-1, 0, 0, 1, viewport_bounds.width(), 0);
    case OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return Transform::Affine(1, 0, 0, -1, 0, viewport_bounds.height());
    case OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
      return Transform::Affine(0, 1, -1, 0, viewport_bounds.height(), 0);
    case OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
      return Transform::Affine(-1, 0, 0, -1, viewport_bounds.width(),
                               viewport_bounds.height());
    case OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
      return Transform::Affine(0, -1, 1, 0, 0, viewport_bounds.width());
    case OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90:
      return Transform::Affine(0, 1, 1, 0, 0, 0);
    case OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270:
      return Transform::Affine(0, -1, -1, 0, viewport_bounds.height(),
                               viewport_bounds.width());
  }

  NOTREACHED_IN_MIGRATION();
  return Transform();
}

OverlayTransform InvertOverlayTransform(OverlayTransform transform) {
  switch (transform) {
    case OVERLAY_TRANSFORM_INVALID:
      NOTREACHED_IN_MIGRATION();
      return OVERLAY_TRANSFORM_NONE;
    case OVERLAY_TRANSFORM_NONE:
      return OVERLAY_TRANSFORM_NONE;
    case OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
      return OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
    case OVERLAY_TRANSFORM_FLIP_VERTICAL:
      return OVERLAY_TRANSFORM_FLIP_VERTICAL;
    case OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
      return OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270;
    case OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
      return OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180;
    case OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
      return OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90;
    case OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90:
      return OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90;
    case OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270:
      return OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270;
  }
  NOTREACHED_IN_MIGRATION();
  return OVERLAY_TRANSFORM_NONE;
}

OverlayTransform OverlayTransformsConcat(OverlayTransform t1,
                                         OverlayTransform t2) {
  if (t1 == OVERLAY_TRANSFORM_INVALID || t2 == OVERLAY_TRANSFORM_INVALID) {
    return OVERLAY_TRANSFORM_INVALID;
  }

  enum VFlip {
    // Written so they behave similar to bools.
    kNo = 0,
    kYes,
  };
  enum Rotation {
    // Enums are written so that it's valid to do modular arithematic.
    // Eg k90 + k270 mod 4 is k0.
    k0 = 0,
    k90,
    k180,
    k270,
  };

  // Step 1: Decompose arguments into vertical flip and clock-wise rotation.
  struct DecomposedOverlayTransform {
    VFlip vflip;
    Rotation rotation;
  };
  auto decompose = [](OverlayTransform t) -> DecomposedOverlayTransform {
    switch (t) {
      case OVERLAY_TRANSFORM_NONE:
        return {VFlip::kNo, Rotation::k0};
      case OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
        return {VFlip::kYes, Rotation::k180};
      case OVERLAY_TRANSFORM_FLIP_VERTICAL:
        return {VFlip::kYes, Rotation::k0};
      case OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90:
        return {VFlip::kNo, Rotation::k90};
      case OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180:
        return {VFlip::kNo, Rotation::k180};
      case OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270:
        return {VFlip::kNo, Rotation::k270};
      case OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90:
        return {VFlip::kYes, Rotation::k90};
      case OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270:
        return {VFlip::kYes, Rotation::k270};
      case OVERLAY_TRANSFORM_INVALID:
        break;
    }
    NOTREACHED();
  };

  DecomposedOverlayTransform decomposed1 = decompose(t1);
  DecomposedOverlayTransform decomposed2 = decompose(t2);

  // Step 2: Compute decomposed result.
  // Result flip is effectively an XOR of two arguments.
  VFlip result_vflip =
      static_cast<VFlip>(decomposed1.vflip != decomposed2.vflip);
  // Add rotation, except that rotation of `t1` needs to be subtracted if `t2`
  // contains a flip.
  Rotation result_rotation;
  if (decomposed2.vflip == VFlip::kYes) {
    result_rotation = static_cast<Rotation>(
        (decomposed2.rotation + 4 - decomposed1.rotation) % 4);
  } else {
    result_rotation = static_cast<Rotation>(
        (decomposed2.rotation + decomposed1.rotation) % 4);
  }

  // Step 3: Reconstruct result.
  switch (result_rotation) {
    case Rotation::k0:
      return result_vflip == VFlip::kYes ? OVERLAY_TRANSFORM_FLIP_VERTICAL
                                         : OVERLAY_TRANSFORM_NONE;
    case Rotation::k90:
      return result_vflip == VFlip::kYes
                 ? OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90
                 : OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90;
    case Rotation::k180:
      return result_vflip == VFlip::kYes
                 ? OVERLAY_TRANSFORM_FLIP_HORIZONTAL
                 : OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180;
    case Rotation::k270:
      return result_vflip == VFlip::kYes
                 ? OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270
                 : OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270;
  }

  NOTREACHED();
}

}  // namespace gfx
