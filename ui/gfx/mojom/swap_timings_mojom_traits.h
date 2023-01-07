// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_SWAP_TIMINGS_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_SWAP_TIMINGS_MOJOM_TRAITS_H_

#include "base/time/time.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/gfx/mojom/swap_timings.mojom-shared.h"
#include "ui/gfx/swap_result.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::SwapTimingsDataView, gfx::SwapTimings> {
  static base::TimeTicks swap_start(const gfx::SwapTimings& input) {
    return input.swap_start;
  }

  static base::TimeTicks swap_end(const gfx::SwapTimings& input) {
    return input.swap_end;
  }

  static bool Read(gfx::mojom::SwapTimingsDataView data,
                   gfx::SwapTimings* out) {
    return data.ReadSwapStart(&out->swap_start) &&
           data.ReadSwapEnd(&out->swap_end);
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_SWAP_TIMINGS_MOJOM_TRAITS_H_
