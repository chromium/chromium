// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_DELEGATED_INK_POINT_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_DELEGATED_INK_POINT_MOJOM_TRAITS_H_

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/gfx/delegated_ink_point.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/delegated_ink_point.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::DelegatedInkPointDataView,
                    gfx::DelegatedInkPoint> {
  static const gfx::PointF& point(const gfx::DelegatedInkPoint& input) {
    return input.point();
  }

  static base::TimeTicks timestamp(const gfx::DelegatedInkPoint& input) {
    return input.timestamp();
  }

  static int32_t pointer_id(const gfx::DelegatedInkPoint& input) {
    return input.pointer_id();
  }

  static bool Read(gfx::mojom::DelegatedInkPointDataView data,
                   gfx::DelegatedInkPoint* out);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_DELEGATED_INK_POINT_MOJOM_TRAITS_H_
