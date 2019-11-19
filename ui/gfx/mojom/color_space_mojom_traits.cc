// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mojom/color_space_mojom_traits.h"

#include "base/stl_util.h"

namespace mojo {

// static
bool StructTraits<gfx::mojom::ColorSpaceDataView, gfx::ColorSpace>::Read(
    gfx::mojom::ColorSpaceDataView input,
    gfx::ColorSpace* out) {
  if (!input.ReadPrimaries(&out->primaries_))
    return false;
  if (!input.ReadTransfer(&out->transfer_))
    return false;
  if (!input.ReadMatrix(&out->matrix_))
    return false;
  if (!input.ReadRange(&out->range_))
    return false;
  {
    base::span<float> matrix(out->custom_primary_matrix_);
    if (!input.ReadCustomPrimaryMatrix(&matrix))
      return false;
  }
  {
    base::span<float> matrix(out->custom_transfer_params_);
    if (!input.ReadCustomTransferParams(&matrix))
      return false;
  }
  return true;
}

}  // namespace mojo
