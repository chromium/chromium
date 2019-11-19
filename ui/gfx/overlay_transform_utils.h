// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OVERLAY_TRANSFORM_UTILS_H_
#define UI_GFX_OVERLAY_TRANSFORM_UTILS_H_

#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/transform.h"

namespace gfx {

GFX_EXPORT gfx::Transform OverlayTransformToTransform(
    gfx::OverlayTransform overlay_transform,
    const gfx::SizeF& viewport_bounds);

GFX_EXPORT gfx::OverlayTransform InvertOverlayTransform(
    gfx::OverlayTransform transform);

}  // namespace gfx

#endif  // UI_GFX_OVERLAY_TRANSFORM_UTILS_H_
