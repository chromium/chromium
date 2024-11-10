// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OVERLAY_TRANSFORM_UTILS_H_
#define UI_GFX_OVERLAY_TRANSFORM_UTILS_H_

#include "base/component_export.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/overlay_transform.h"

namespace gfx {

COMPONENT_EXPORT(GFX)
Transform OverlayTransformToTransform(OverlayTransform overlay_transform,
                                      const SizeF& viewport_bounds);

COMPONENT_EXPORT(GFX)
OverlayTransform InvertOverlayTransform(OverlayTransform transform);

// Returns result of applying `t1`, followed by `t2`. May return invalid if
// there inputs contain invalid.
COMPONENT_EXPORT(GFX)
OverlayTransform OverlayTransformsConcat(OverlayTransform t1,
                                         OverlayTransform t2);

}  // namespace gfx

#endif  // UI_GFX_OVERLAY_TRANSFORM_UTILS_H_
