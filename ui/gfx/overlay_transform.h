// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OVERLAY_TRANSFORM_H_
#define UI_GFX_OVERLAY_TRANSFORM_H_

#include <stdint.h>

namespace gfx {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.gfx
// Describes transformation to be applied to the buffer before presenting
// to screen. Rotations are expressed in clockwise degrees.
enum OverlayTransform : uint8_t {
  OVERLAY_TRANSFORM_INVALID,
  OVERLAY_TRANSFORM_NONE,
  OVERLAY_TRANSFORM_FLIP_HORIZONTAL,
  OVERLAY_TRANSFORM_FLIP_VERTICAL,
  OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
  OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180,
  OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270,
  // For these two, flip first, rotate second.
  OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_90,
  OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270,
  OVERLAY_TRANSFORM_LAST = OVERLAY_TRANSFORM_FLIP_VERTICAL_CLOCKWISE_270
};

}  // namespace gfx

#endif  // UI_GFX_OVERLAY_TRANSFORM_H_
