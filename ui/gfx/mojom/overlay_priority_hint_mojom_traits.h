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
    NOTREACHED();
  }

  static gfx::OverlayPriorityHint FromMojom(
      gfx::mojom::OverlayPriorityHint input) {
    switch (input) {
      case gfx::mojom::OverlayPriorityHint::kNone:
        return gfx::OverlayPriorityHint::kNone;
      case gfx::mojom::OverlayPriorityHint::kRegular:
        return gfx::OverlayPriorityHint::kRegular;
      case gfx::mojom::OverlayPriorityHint::kLowLatencyCanvas:
        return gfx::OverlayPriorityHint::kLowLatencyCanvas;
      case gfx::mojom::OverlayPriorityHint::kHardwareProtection:
        return gfx::OverlayPriorityHint::kHardwareProtection;
      case gfx::mojom::OverlayPriorityHint::kVideo:
        return gfx::OverlayPriorityHint::kVideo;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_OVERLAY_PRIORITY_HINT_MOJOM_TRAITS_H_
