// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_MASK_FILTER_INFO_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_MASK_FILTER_INFO_MOJOM_TRAITS_H_

#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/mojom/linear_gradient_mojom_traits.h"
#include "ui/gfx/mojom/mask_filter_info.mojom-shared.h"
#include "ui/gfx/mojom/rrect_f_mojom_traits.h"

namespace mojo {
template <>
struct StructTraits<gfx::mojom::MaskFilterInfoDataView, gfx::MaskFilterInfo> {
  static const gfx::RRectF& rounded_corner_bounds(
      const gfx::MaskFilterInfo& info) {
    return info.rounded_corner_bounds();
  }

  static const std::optional<gfx::LinearGradient>& gradient_mask(
      const gfx::MaskFilterInfo& info) {
    return info.gradient_mask();
  }

  static bool Read(gfx::mojom::MaskFilterInfoDataView data,
                   gfx::MaskFilterInfo* out);
};

}  // namespace mojo
#endif  // UI_GFX_MOJOM_MASK_FILTER_INFO_MOJOM_TRAITS_H_
