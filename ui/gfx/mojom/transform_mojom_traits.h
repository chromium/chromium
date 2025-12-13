// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_

#include <array>

#include "base/check.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "mojo/public/cpp/bindings/union_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/mojom/transform.mojom-shared.h"

namespace mojo {

template <>
struct UnionTraits<gfx::mojom::TransformDataDataView, gfx::Transform> {
  static bool IsNull(const gfx::Transform& transform) {
    return transform.IsIdentity();
  }

  static void SetToNull(gfx::Transform* out) { out->MakeIdentity(); }

  static gfx::mojom::TransformDataDataView::Tag GetTag(
      const gfx::Transform& transform) {
    if (transform.full_matrix_) {
      return gfx::mojom::TransformDataDataView::Tag::kMatrix;
    }
    return gfx::mojom::TransformDataDataView::Tag::kAxis2d;
  }

  static const gfx::AxisTransform2d& axis_2d(const gfx::Transform& transform) {
    DCHECK(!transform.full_matrix_);
    return transform.axis_2d_;
  }

  static std::array<double, 16> matrix(const gfx::Transform& transform) {
    DCHECK(transform.full_matrix_);
    std::array<double, 16> matrix_array;
    transform.GetColMajor(matrix_array);
    return matrix_array;
  }

  static bool Read(gfx::mojom::TransformDataDataView data,
                   gfx::Transform* out) {
    switch (data.tag()) {
      case gfx::mojom::TransformDataDataView::Tag::kAxis2d: {
        gfx::AxisTransform2d axis_2d;
        if (!data.ReadAxis2d(&axis_2d)) {
          return false;
        }
        *out = gfx::Transform(axis_2d);
        return true;
      }
      case gfx::mojom::TransformDataDataView::Tag::kMatrix: {
        ArrayDataView<double> matrix_data;
        data.GetMatrixDataView(&matrix_data);
        *out = gfx::Transform::ColMajor(base::span(matrix_data).first<16>());
        return true;
      }
    }
    return false;
  }
};

template <>
struct StructTraits<gfx::mojom::TransformDataView, gfx::Transform> {
  static const gfx::Transform& data(const gfx::Transform& transform) {
    return transform;
  }

  static bool Read(gfx::mojom::TransformDataView data, gfx::Transform* out) {
    return data.ReadData(out);
  }
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_
