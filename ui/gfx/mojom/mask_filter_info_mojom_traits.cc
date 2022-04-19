// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/mask_filter_info_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<gfx::mojom::MaskFilterInfoDataView, gfx::MaskFilterInfo>::
    Read(gfx::mojom::MaskFilterInfoDataView data, gfx::MaskFilterInfo* out) {
  gfx::RRectF bounds;
  if (!data.ReadRoundedCornerBounds(&bounds))
    return false;
  *out = gfx::MaskFilterInfo(bounds);
  return true;
}

}  // namespace mojo
