// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_OVERLAY_PRIORITY_HINT_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_OVERLAY_PRIORITY_HINT_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "ui/gfx/mojom/overlay_priority_hint.mojom.h"
#include "ui/gfx/overlay_priority_hint.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::OverlayPriorityHint, gfx::OverlayPriorityHint> {
  static gfx::mojom::OverlayPriorityHint ToMojom(
      gfx::OverlayPriorityHint hint) {
    switch (hint) {
      case gfx::OverlayPriorityHint::kNone:
        return gfx::mojom::OverlayPriorityHint::kNone;
      case gfx::OverlayPriorityHint::kRegular:
        return gfx::mojom::OverlayPriorityHint::kRegular;
      case gfx::OverlayPriorityHint::kLowLatencyCanvas:
        return gfx::mojom::OverlayPriorityHint::kLowLatencyCanvas;
      case gfx::OverlayPriorityHint::kHardwareProtection:
        return gfx::mojom::OverlayPriorityHint::kHardwareProtection;
      case gfx::OverlayPriorityHint::kVideo:
        return gfx::mojom::OverlayPriorityHint::kVideo;
    }
    NOTREACHED_IN_MIGRATION();
    return gfx::mojom::OverlayPriorityHint::kNone;
  }

  static bool FromMojom(gfx::mojom::OverlayPriorityHint input,
                        gfx::OverlayPriorityHint* out) {
    switch (input) {
      case gfx::mojom::OverlayPriorityHint::kNone:
        *out = gfx::OverlayPriorityHint::kNone;
        return true;
      case gfx::mojom::OverlayPriorityHint::kRegular:
        *out = gfx::OverlayPriorityHint::kRegular;
        return true;
      case gfx::mojom::OverlayPriorityHint::kLowLatencyCanvas:
        *out = gfx::OverlayPriorityHint::kLowLatencyCanvas;
        return true;
      case gfx::mojom::OverlayPriorityHint::kHardwareProtection:
        *out = gfx::OverlayPriorityHint::kHardwareProtection;
        return true;
      case gfx::mojom::OverlayPriorityHint::kVideo:
        *out = gfx::OverlayPriorityHint::kVideo;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_OVERLAY_PRIORITY_HINT_MOJOM_TRAITS_H_
