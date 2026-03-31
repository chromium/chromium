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
      case gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS:
        return gfx::mojom::SwapResult::NAK_RECREATE_BUFFERS;
    }
    NOTREACHED();
  }

  static gfx::SwapResult FromMojom(gfx::mojom::SwapResult input) {
    switch (input) {
      case gfx::mojom::SwapResult::ACK:
        return gfx::SwapResult::SWAP_ACK;
      case gfx::mojom::SwapResult::FAILED:
        return gfx::SwapResult::SWAP_FAILED;
      case gfx::mojom::SwapResult::SKIPPED:
        return gfx::SwapResult::SWAP_SKIPPED;
      case gfx::mojom::SwapResult::NAK_RECREATE_BUFFERS:
        return gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_SWAP_RESULT_MOJOM_TRAITS_H_
