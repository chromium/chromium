// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OVERLAY_TRANSFORM_UTILS_H_
#define UI_GFX_OVERLAY_TRANSFORM_UTILS_H_

#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {

GFX_EXPORT Transform
OverlayTransformToTransform(OverlayTransform overlay_transform,
                            const SizeF& viewport_bounds);

GFX_EXPORT OverlayTransform InvertOverlayTransform(OverlayTransform transform);

// Returns result of applying `t1`, followed by `t2`. May return invalid if
// there inputs contain invalid.
GFX_EXPORT OverlayTransform OverlayTransformsConcat(OverlayTransform t1,
                                                    OverlayTransform t2);

}  // namespace gfx

#endif  // UI_GFX_OVERLAY_TRANSFORM_UTILS_H_
