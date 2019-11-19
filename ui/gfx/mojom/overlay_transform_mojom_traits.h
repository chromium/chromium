// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_OVERLAY_TRANSFORM_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_OVERLAY_TRANSFORM_MOJOM_TRAITS_H_

#include "ui/gfx/mojom/overlay_transform.mojom.h"
#include "ui/gfx/overlay_transform.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::OverlayTransform, gfx::OverlayTransform> {
  static gfx::mojom::OverlayTransform ToMojom(gfx::OverlayTransform format) {
    switch (format) {
      case gfx::OverlayTransform::OVERLAY_TRANSFORM_INVALID:
        return gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_INVALID;
      case gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE:
        return gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_NONE;
      case gfx::OverlayTransform::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
        return gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
      case gfx::OverlayTransform::OVERLAY_TRANSFORM_FLIP_VERTICAL:
        return gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_FLIP_VERTICAL;
      case gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_90:
        return gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_90;
      case gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_180:
        return gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_180;
      case gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_270:
        return gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_270;
    }
    NOTREACHED();
    return gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_INVALID;
  }

  static bool FromMojom(gfx::mojom::OverlayTransform input,
                        gfx::OverlayTransform* out) {
    switch (input) {
      case gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_INVALID:
        *out = gfx::OverlayTransform::OVERLAY_TRANSFORM_INVALID;
        return true;
      case gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_NONE:
        *out = gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;
        return true;
      case gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_FLIP_HORIZONTAL:
        *out = gfx::OverlayTransform::OVERLAY_TRANSFORM_FLIP_HORIZONTAL;
        return true;
      case gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_FLIP_VERTICAL:
        *out = gfx::OverlayTransform::OVERLAY_TRANSFORM_FLIP_VERTICAL;
        return true;
      case gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_90:
        *out = gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_90;
        return true;
      case gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_180:
        *out = gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_180;
        return true;
      case gfx::mojom::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_270:
        *out = gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_270;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_OVERLAY_TRANSFORM_MOJOM_TRAITS_H_
