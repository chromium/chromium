// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_SWAP_RESULT_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_SWAP_RESULT_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "ui/gfx/mojom/swap_result.mojom-shared.h"
#include "ui/gfx/swap_result.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::SwapResult, gfx::SwapResult> {
  static gfx::mojom::SwapResult ToMojom(gfx::SwapResult input) {
    switch (input) {
      case gfx::SwapResult::SWAP_ACK:
        return gfx::mojom::SwapResult::ACK;
      case gfx::SwapResult::SWAP_FAILED:
        return gfx::mojom::SwapResult::FAILED;
      case gfx::SwapResult::SWAP_SKIPPED:
        return gfx::mojom::SwapResult::SKIPPED;
      case gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED:
        return gfx::mojom::SwapResult::NON_SIMPLE_OVERLAYS_FAILED;
      case gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS:
        return gfx::mojom::SwapResult::NAK_RECREATE_BUFFERS;
    }
    NOTREACHED_IN_MIGRATION();
    return gfx::mojom::SwapResult::FAILED;
  }

  static bool FromMojom(gfx::mojom::SwapResult input, gfx::SwapResult* out) {
    switch (input) {
      case gfx::mojom::SwapResult::ACK:
        *out = gfx::SwapResult::SWAP_ACK;
        return true;
      case gfx::mojom::SwapResult::FAILED:
        *out = gfx::SwapResult::SWAP_FAILED;
        return true;
      case gfx::mojom::SwapResult::SKIPPED:
        *out = gfx::SwapResult::SWAP_SKIPPED;
        return true;
      case gfx::mojom::SwapResult::NON_SIMPLE_OVERLAYS_FAILED:
        *out = gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED;
        return true;
      case gfx::mojom::SwapResult::NAK_RECREATE_BUFFERS:
        *out = gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_SWAP_RESULT_MOJOM_TRAITS_H_
