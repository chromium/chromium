// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/mask_filter_info.h"

#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {

void MaskFilterInfo::ApplyTransform(const Transform& transform) {
  if (rounded_corner_bounds_.IsEmpty()) {
    return;
  }

  // We want this to fail only in cases where our
  // Transform::Preserves2dAxisAlignment() returns false.  However,
  // SkMatrix::preservesAxisAlignment() is stricter (it lacks the kEpsilon
  // test).  So after converting our Matrix44 to SkMatrix, round
  // relevant values less than kEpsilon to zero.
  constexpr float kEpsilon = std::numeric_limits<float>::epsilon();
  SkMatrix rounded_matrix = TransformToFlattenedSkMatrix(transform);
  if (std::abs(rounded_matrix.get(SkMatrix::kMScaleX)) < kEpsilon)
    rounded_matrix.set(SkMatrix::kMScaleX, 0.0f);
  if (std::abs(rounded_matrix.get(SkMatrix::kMSkewX)) < kEpsilon)
    rounded_matrix.set(SkMatrix::kMSkewX, 0.0f);
  if (std::abs(rounded_matrix.get(SkMatrix::kMSkewY)) < kEpsilon)
    rounded_matrix.set(SkMatrix::kMSkewY, 0.0f);
  if (std::abs(rounded_matrix.get(SkMatrix::kMScaleY)) < kEpsilon)
    rounded_matrix.set(SkMatrix::kMScaleY, 0.0f);

  SkRRect new_rect;
  if (!SkRRect(rounded_corner_bounds_).transform(rounded_matrix, &new_rect) ||
      !new_rect.isValid()) {
    rounded_corner_bounds_ = RRectF();
    return;
  }
  rounded_corner_bounds_ = RRectF(new_rect);

  if (gradient_mask_ && !gradient_mask_->IsEmpty()) {
    gradient_mask_->ApplyTransform(transform);
  }
}

void MaskFilterInfo::ApplyTransform(const AxisTransform2d& transform) {
  if (rounded_corner_bounds_.IsEmpty())
    return;

  rounded_corner_bounds_.Scale(transform.scale().x(), transform.scale().y());
  rounded_corner_bounds_.Offset(transform.translation());
  if (!SkRRect(rounded_corner_bounds_).isValid()) {
    rounded_corner_bounds_ = RRectF();
    return;
  }

  if (gradient_mask_ && !gradient_mask_->IsEmpty()) {
    gradient_mask_->ApplyTransform(transform);
  }
}

std::string MaskFilterInfo::ToString() const {
  std::string result = "MaskFilterInfo{" + rounded_corner_bounds_.ToString();

  if (gradient_mask_)
    result += ", gradient_mask=" + gradient_mask_->ToString();

  result += "}";

  return result;
}

}  // namespace gfx
