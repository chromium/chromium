// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_SWAP_RESULT_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_SWAP_RESULT_MOJOM_TRAITS_H_

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
      case gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS:
        return gfx::mojom::SwapResult::NAK_RECREATE_BUFFERS;
    }
    NOTREACHED();
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
      case gfx::mojom::SwapResult::NAK_RECREATE_BUFFERS:
        *out = gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_SWAP_RESULT_MOJOM_TRAITS_H_
