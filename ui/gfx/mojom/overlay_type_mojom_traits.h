// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_OVERLAY_TYPE_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_OVERLAY_TYPE_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "ui/gfx/mojom/overlay_type.mojom.h"
#include "ui/gfx/overlay_type.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::OverlayType, gfx::OverlayType> {
  static gfx::mojom::OverlayType ToMojom(gfx::OverlayType format) {
    switch (format) {
      case gfx::OverlayType::kSimple:
        return gfx::mojom::OverlayType::kSimple;
      case gfx::OverlayType::kUnderlay:
        return gfx::mojom::OverlayType::kUnderlay;
      case gfx::OverlayType::kSingleOnTop:
        return gfx::mojom::OverlayType::kSingleOnTop;
      case gfx::OverlayType::kFullScreen:
        return gfx::mojom::OverlayType::kFullScreen;
    }
    NOTREACHED();
    return gfx::mojom::OverlayType::kSimple;
  }

  static gfx::OverlayType FromMojom(gfx::mojom::OverlayType input) {
    switch (input) {
      case gfx::mojom::OverlayType::kSimple:
        return gfx::OverlayType::kSimple;
      case gfx::mojom::OverlayType::kUnderlay:
        return gfx::OverlayType::kUnderlay;
      case gfx::mojom::OverlayType::kSingleOnTop:
        return gfx::OverlayType::kSingleOnTop;
      case gfx::mojom::OverlayType::kFullScreen:
        return gfx::OverlayType::kFullScreen;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_OVERLAY_TYPE_MOJOM_TRAITS_H_
