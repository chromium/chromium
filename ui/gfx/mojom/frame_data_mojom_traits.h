// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_FRAME_DATA_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_FRAME_DATA_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/mojom/frame_data.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::FrameDataDataView, gfx::FrameData> {
  static int64_t seq(const gfx::FrameData& data) { return data.seq; }
  static int64_t swap_trace_id(const gfx::FrameData& data) {
    return data.swap_trace_id;
  }

  static bool Read(gfx::mojom::FrameDataDataView data, gfx::FrameData* out) {
    out->seq = data.seq();
    out->swap_trace_id = data.swap_trace_id();
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_FRAME_DATA_MOJOM_TRAITS_H_
