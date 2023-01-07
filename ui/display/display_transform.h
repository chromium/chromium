// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_DISPLAY_TRANSFORM_H_
#define UI_DISPLAY_DISPLAY_TRANSFORM_H_

#include "ui/display/display.h"
#include "ui/display/display_export.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {
class SizeF;
class Transform;
}  // namespace gfx

namespace display {

// Creates an exact gfx::Transform for a rotation.
DISPLAY_EXPORT gfx::Transform CreateRotationTransform(
    display::Display::Rotation rotation,
    const gfx::SizeF& size_to_rotate);

// Maps display::Display::Rotation to gfx::OverlayTransform.
DISPLAY_EXPORT gfx::OverlayTransform DisplayRotationToOverlayTransform(
    display::Display::Rotation rotation);

}  // namespace display

#endif  // UI_DISPLAY_DISPLAY_TRANSFORM_H_
