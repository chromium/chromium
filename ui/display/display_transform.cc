// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/display_transform.h"

#include "base/notreached.h"
#include "ui/gfx/overlay_transform_utils.h"

namespace display {

gfx::Transform CreateRotationTransform(display::Display::Rotation rotation,
                                       const gfx::SizeF& size_to_rotate) {
  return OverlayTransformToTransform(
      DisplayRotationToOverlayTransform(rotation), size_to_rotate);
}

gfx::OverlayTransform DisplayRotationToOverlayTransform(
    display::Display::Rotation rotation) {
  // Note that the angle provided by |rotation| here is the opposite direction
  // of the physical rotation of the device, which is the space in which the UI
  // prepares the scene (see
  // https://developer.android.com/reference/android/view/Display#getRotation()
  // for details).
  //
  // The rotation which needs to be applied by the display compositor to allow
  // the buffers produced by it to be used directly by the system compositor
  // needs to be the inverse of this rotation. Since display::Rotation is in
  // clockwise direction while gfx::OverlayTransform is anti-clockwise, directly
  // mapping them below performs this inversion.
  switch (rotation) {
    case display::Display::ROTATE_0:
      return gfx::OVERLAY_TRANSFORM_NONE;
    case display::Display::ROTATE_90:
      return gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90;
    case display::Display::ROTATE_180:
      return gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_180;
    case display::Display::ROTATE_270:
      return gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270;
  }
  NOTREACHED_IN_MIGRATION();
  return gfx::OVERLAY_TRANSFORM_NONE;
}

}  // namespace display
