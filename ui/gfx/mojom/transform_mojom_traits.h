// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/array_traits.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/mojom/transform.mojom-shared.h"

namespace mojo {

template <>
struct StructTraits<gfx::mojom::TransformDataView, gfx::Transform> {
  static std::optional<std::array<float, 16>> matrix(
      const gfx::Transform& transform) {
    if (transform.IsIdentity())
      return std::nullopt;
    std::array<float, 16> matrix;
    transform.GetColMajorF(matrix.data());
    return matrix;
  }

  static bool Read(gfx::mojom::TransformDataView data, gfx::Transform* out) {
    ArrayDataView<float> matrix;
    data.GetMatrixDataView(&matrix);
    if (matrix.is_null()) {
      out->MakeIdentity();
      return true;
    }
    *out = gfx::Transform::ColMajorF(matrix.data());
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_
