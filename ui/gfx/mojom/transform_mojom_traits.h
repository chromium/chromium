// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/array_traits.h"
#include "ui/gfx/mojom/transform.mojom-shared.h"
#include "ui/gfx/transform.h"

namespace mojo {

template <>
struct ArrayTraits<SkMatrix44> {
  using Element = float;

  static bool IsNull(const SkMatrix44& input) { return input.isIdentity(); }

  static size_t GetSize(const SkMatrix44& input) { return 16; }

  static float GetAt(const SkMatrix44& input, size_t index) {
    return input.getFloat(static_cast<int>(index % 4),
                          static_cast<int>(index / 4));
  }
};

template <>
struct StructTraits<gfx::mojom::TransformDataView, gfx::Transform> {
  static const SkMatrix44& matrix(const gfx::Transform& transform) {
    return transform.matrix();
  }

  static bool Read(gfx::mojom::TransformDataView data, gfx::Transform* out) {
    ArrayDataView<float> matrix;
    data.GetMatrixDataView(&matrix);
    if (matrix.is_null()) {
      out->MakeIdentity();
      return true;
    }
    out->matrix().setColMajorf(matrix.data());
    return true;
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_
