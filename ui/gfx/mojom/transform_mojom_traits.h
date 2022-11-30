// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_TRANSFORM_MOJOM_TRAITS_H_

#include "base/record_replay.h"
#include "mojo/public/cpp/bindings/array_traits.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/mojom/transform.mojom-shared.h"

namespace mojo {

template <>
<<<<<<< HEAD
struct ArrayTraits<SkMatrix44> {
  using Element = float;

  static bool IsNull(const SkMatrix44& input) {
    // When recording/replaying, whether a matrix is an identity or not can
    // vary between recording and replaying for unknown reasons. Checking
    // isIdentity below can lead to messages having different lengths,
    // so for now we workaround this by always serializing the matrix.
    if (recordreplay::IsRecordingOrReplaying()) {
      return false;
    }
    return input.isIdentity();
  }

  static size_t GetSize(const SkMatrix44& input) { return 16; }

  static float GetAt(const SkMatrix44& input, size_t index) {
    return input.getFloat(static_cast<int>(index % 4),
                          static_cast<int>(index / 4));
  }
};

template <>
||||||| 80c960997e61f
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
=======
>>>>>>> 27d3765d341b09369006d030f83f582a29eb57ae
struct StructTraits<gfx::mojom::TransformDataView, gfx::Transform> {
  static absl::optional<std::array<float, 16>> matrix(
      const gfx::Transform& transform) {
    if (transform.IsIdentity())
      return absl::nullopt;
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
