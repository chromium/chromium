// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_INK_DROP_UTIL_H_
#define UI_VIEWS_ANIMATION_INK_DROP_UTIL_H_

#include "ui/views/views_export.h"

namespace gfx {
class Transform;
}  // namespace gfx

namespace views {
class View;

// A layer |transform| may add an offset to its layer relative to the parent
// layer. This offset does not take into consideration the subpixel positioning.
// A subpixel correction needs to be applied to make sure the layers are pixel
// aligned after the transform is applied. Use this function to compute the
// subpixel correction transform.
VIEWS_EXPORT gfx::Transform GetTransformSubpixelCorrection(
    const gfx::Transform& transform,
    float device_scale_factor);

VIEWS_EXPORT bool UsingPlatformHighContrastInkDrop(const View* view);

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_INK_DROP_UTIL_H_
