// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/transform_operations.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/numerics/angle_conversions.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {

TransformOperations::TransformOperations() = default;

TransformOperations::TransformOperations(const TransformOperations& other) {
  operations_ = other.operations_;
}

TransformOperations::~TransformOperations() = default;

TransformOperations& TransformOperations::operator=(
    const TransformOperations& other) {
  operations_ = other.operations_;
  return *this;
}

Transform TransformOperations::Apply() const {
  return ApplyRemaining(0);
}

Transform TransformOperations::ApplyRemaining(size_t start) const {
  Transform to_return;
  for (size_t i = start; i < operations_.size(); i++) {
    to_return.PreConcat(operations_[i].matrix);
  }
  return to_return;
}

// TODO(crbug.com/41431421): Consolidate blink and cc implementations of
// transform interpolation.
TransformOperations TransformOperations::Blend(const TransformOperations& from,
                                               SkScalar progress) const {
  TransformOperations to_return;
  if (!BlendInternal(from, progress, &to_return)) {
    // If the matrices cannot be blended, fallback to discrete animation logic.
    // See https://drafts.csswg.org/css-transforms/#matrix-interpolation
    to_return = progress < 0.5 ? from : *this;
  }
  return to_return;
}

bool TransformOperations::BlendedBoundsForBox(const BoxF& box,
                                              const TransformOperations& from,
                                              SkScalar min_progress,
                                              SkScalar max_progress,
                                              BoxF* bounds) const {
  *bounds = box;

  bool from_identity = from.IsIdentity();
  bool to_identity = IsIdentity();
  if (from_identity && to_identity)
    return true;

  if (!MatchesTypes(from))
    return false;

  size_t num_operations = std::max(from_identity ? 0 : from.operations_.size(),
                                   to_identity ? 0 : operations_.size());

  // Because we are squashing all of the matrices together when applying
  // them to the animation, we must apply them in reverse order when
  // not squashing them.
  for (size_t i = 0; i < num_operations; ++i) {
    size_t operation_index = num_operations - 1 - i;
    BoxF bounds_for_operation;
    const TransformOperation* from_op =
        from_identity ? nullptr : &from.operations_[operation_index];
    const TransformOperation* to_op =
        to_identity ? nullptr : &operations_[operation_index];
    if (!TransformOperation::BlendedBoundsForBox(*bounds, from_op, to_op,
                                                 min_progress, max_progress,
                                                 &bounds_for_operation)) {
      return false;
    }
    *bounds = bounds_for_operation;
  }

  return true;
}

bool TransformOperations::PreservesAxisAlignment() const {
  for (auto& operation : operations_) {
    switch (operation.type) {
      case TransformOperation::TRANSFORM_OPERATION_IDENTITY:
      case TransformOperation::TRANSFORM_OPERATION_TRANSLATE:
      case TransformOperation::TRANSFORM_OPERATION_SCALE:
        continue;
      case TransformOperation::TRANSFORM_OPERATION_MATRIX:
        if (!operation.matrix.IsIdentity() &&
            !operation.matrix.IsScaleOrTranslation())
          return false;
        continue;
      case TransformOperation::TRANSFORM_OPERATION_ROTATE:
      case TransformOperation::TRANSFORM_OPERATION_SKEWX:
      case TransformOperation::TRANSFORM_OPERATION_SKEWY:
      case TransformOperation::TRANSFORM_OPERATION_SKEW:
      case TransformOperation::TRANSFORM_OPERATION_PERSPECTIVE:
        return false;
    }
  }
  return true;
}

bool TransformOperations::IsTranslation() const {
  for (auto& operation : operations_) {
    switch (operation.type) {
      case TransformOperation::TRANSFORM_OPERATION_IDENTITY:
      case TransformOperation::TRANSFORM_OPERATION_TRANSLATE:
        continue;
      case TransformOperation::TRANSFORM_OPERATION_MATRIX:
        if (!operation.matrix.IsIdentityOrTranslation())
          return false;
        continue;
      case TransformOperation::TRANSFORM_OPERATION_ROTATE:
      case TransformOperation::TRANSFORM_OPERATION_SCALE:
      case TransformOperation::TRANSFORM_OPERATION_SKEWX:
      case TransformOperation::TRANSFORM_OPERATION_SKEWY:
      case TransformOperation::TRANSFORM_OPERATION_SKEW:
      case TransformOperation::TRANSFORM_OPERATION_PERSPECTIVE:
        return false;
    }
  }
  return true;
}

static SkScalar TanDegrees(double degrees) {
  return SkDoubleToScalar(std::tan(base::DegToRad(degrees)));
}

bool TransformOperations::ScaleComponent(SkScalar* scale) const {
  SkScalar operations_scale = 1.f;
  for (auto& operation : operations_) {
    switch (operation.type) {
      case TransformOperation::TRANSFORM_OPERATION_IDENTITY:
      case TransformOperation::TRANSFORM_OPERATION_TRANSLATE:
      case TransformOperation::TRANSFORM_OPERATION_ROTATE:
        continue;
      case TransformOperation::TRANSFORM_OPERATION_MATRIX: {
        if (operation.matrix.HasPerspective())
          return false;
        Vector2dF scale_components =
            ComputeTransform2dScaleComponents(operation.matrix, 1.f);
        operations_scale *=
            std::max(scale_components.x(), scale_components.y());
        break;
      }
      case TransformOperation::TRANSFORM_OPERATION_SKEWX:
      case TransformOperation::TRANSFORM_OPERATION_SKEWY:
      case TransformOperation::TRANSFORM_OPERATION_SKEW: {
        SkScalar x_component = TanDegrees(operation.skew.x);
        SkScalar y_component = TanDegrees(operation.skew.y);
        SkScalar x_scale = std::sqrt(x_component * x_component + 1);
        SkScalar y_scale = std::sqrt(y_component * y_component + 1);
        operations_scale *= std::max(x_scale, y_scale);
        break;
      }
      case TransformOperation::TRANSFORM_OPERATION_PERSPECTIVE:
        return false;
      case TransformOperation::TRANSFORM_OPERATION_SCALE:
        operations_scale *= std::max(
            std::abs(operation.scale.x),
            std::max(std::abs(operation.scale.y), std::abs(operation.scale.z)));
    }
  }
  *scale = operations_scale;
  return true;
}

bool TransformOperations::MatchesTypes(const TransformOperations& other) const {
  if (operations_.size() == 0 || other.operations_.size() == 0)
    return true;

  if (operations_.size() != other.operations_.size())
    return false;

  for (size_t i = 0; i < operations_.size(); ++i) {
    if (operations_[i].type != other.operations_[i].type)
      return false;
  }

  return true;
}

size_t TransformOperations::MatchingPrefixLength(
    const TransformOperations& other) const {
  size_t num_operations =
      std::min(operations_.size(), other.operations_.size());
  for (size_t i = 0; i < num_operations; ++i) {
    if (operations_[i].type != other.operations_[i].type) {
      // Remaining operations in each operations list require matrix/matrix3d
      // interpolation.
      return i;
    }
  }
  // If the operations match to the length of the shorter list, then pad its
  // length with the matching identity operations.
  // https://drafts.csswg.org/css-transforms/#transform-function-lists
  return std::max(operations_.size(), other.operations_.size());
}

bool TransformOperations::CanBlendWith(const TransformOperations& other) const {
  TransformOperations dummy;
  return BlendInternal(other, 0.5, &dummy);
}

void TransformOperations::AppendTranslate(SkScalar x, SkScalar y, SkScalar z) {
  TransformOperation to_add;
  to_add.matrix.Translate3d(x, y, z);
  to_add.type = TransformOperation::TRANSFORM_OPERATION_TRANSLATE;
  to_add.translate.x = x;
  to_add.translate.y = y;
  to_add.translate.z = z;
  operations_.push_back(to_add);
  decomposed_transforms_.clear();
}

void TransformOperations::AppendRotate(SkScalar x,
                                       SkScalar y,
                                       SkScalar z,
                                       SkScalar degrees) {
  TransformOperation to_add;
  to_add.type = TransformOperation::TRANSFORM_OPERATION_ROTATE;
  to_add.rotate.axis.x = x;
  to_add.rotate.axis.y = y;
  to_add.rotate.axis.z = z;
  to_add.rotate.angle = degrees;
  to_add.Bake();
  operations_.push_back(to_add);
  decomposed_transforms_.clear();
}

void TransformOperations::AppendScale(SkScalar x, SkScalar y, SkScalar z) {
  TransformOperation to_add;
  to_add.type = TransformOperation::TRANSFORM_OPERATION_SCALE;
  to_add.scale.x = x;
  to_add.scale.y = y;
  to_add.scale.z = z;
  to_add.Bake();
  operations_.push_back(to_add);
  decomposed_transforms_.clear();
}

void TransformOperations::AppendSkewX(SkScalar x) {
  TransformOperation to_add;
  to_add.type = TransformOperation::TRANSFORM_OPERATION_SKEWX;
  to_add.skew.x = x;
  to_add.skew.y = 0;
  to_add.Bake();
  operations_.push_back(to_add);
  decomposed_transforms_.clear();
}

void TransformOperations::AppendSkewY(SkScalar y) {
  TransformOperation to_add;
  to_add.type = TransformOperation::TRANSFORM_OPERATION_SKEWY;
  to_add.skew.x = 0;
  to_add.skew.y = y;
  to_add.Bake();
  operations_.push_back(to_add);
  decomposed_transforms_.clear();
}

void TransformOperations::AppendSkew(SkScalar x, SkScalar y) {
  TransformOperation to_add;
  to_add.type = TransformOperation::TRANSFORM_OPERATION_SKEW;
  to_add.skew.x = x;
  to_add.skew.y = y;
  to_add.Bake();
  operations_.push_back(to_add);
  decomposed_transforms_.clear();
}

void TransformOperations::AppendPerspective(std::optional<SkScalar> depth) {
  TransformOperation to_add;
  to_add.type = TransformOperation::TRANSFORM_OPERATION_PERSPECTIVE;
  if (depth) {
    DCHECK_GE(*depth, 1.0f);
    to_add.perspective_m43 = -1.0f / *depth;
  } else {
    to_add.perspective_m43 = 0.0f;
  }
  to_add.Bake();
  operations_.push_back(to_add);
  decomposed_transforms_.clear();
}

void TransformOperations::AppendMatrix(const Transform& matrix) {
  TransformOperation to_add;
  to_add.matrix = matrix;
  to_add.type = TransformOperation::TRANSFORM_OPERATION_MATRIX;
  operations_.push_back(to_add);
  decomposed_transforms_.clear();
}

void TransformOperations::AppendIdentity() {
  operations_.emplace_back();
}

void TransformOperations::Append(const TransformOperation& operation) {
  operations_.push_back(operation);
  decomposed_transforms_.clear();
}

bool TransformOperations::IsIdentity() const {
  for (auto& operation : operations_) {
    if (!operation.IsIdentity())
      return false;
  }
  return true;
}

bool TransformOperations::ApproximatelyEqual(const TransformOperations& other,
                                             SkScalar tolerance) const {
  if (size() != other.size())
    return false;
  for (size_t i = 0; i < operations_.size(); ++i) {
    if (!operations_[i].ApproximatelyEqual(other.operations_[i], tolerance))
      return false;
  }
  return true;
}

bool TransformOperations::BlendInternal(const TransformOperations& from,
                                        SkScalar progress,
                                        TransformOperations* result) const {
  bool from_identity = from.IsIdentity();
  bool to_identity = IsIdentity();
  if (from_identity && to_identity)
    return true;

  size_t matching_prefix_length = MatchingPrefixLength(from);
  size_t from_size = from_identity ? 0 : from.operations_.size();
  size_t to_size = to_identity ? 0 : operations_.size();
  size_t num_operations = std::max(from_size, to_size);

  for (size_t i = 0; i < matching_prefix_length; ++i) {
    TransformOperation blended;
    if (!TransformOperation::BlendTransformOperations(
            i >= from_size ? nullptr : &from.operations_[i],
            i >= to_size ? nullptr : &operations_[i], progress, &blended)) {
      return false;
    }
    result->Append(blended);
  }

  if (matching_prefix_length < num_operations) {
    if (!ComputeDecomposedTransform(matching_prefix_length) ||
        !from.ComputeDecomposedTransform(matching_prefix_length)) {
      return false;
    }
    DecomposedTransform matrix_transform = BlendDecomposedTransforms(
        *decomposed_transforms_[matching_prefix_length].get(),
        *from.decomposed_transforms_[matching_prefix_length].get(), progress);
    result->AppendMatrix(Transform::Compose(matrix_transform));
  }
  return true;
}

bool TransformOperations::ComputeDecomposedTransform(
    size_t start_offset) const {
  auto it = decomposed_transforms_.find(start_offset);
  if (it == decomposed_transforms_.end()) {
    Transform transform = ApplyRemaining(start_offset);
    if (std::optional<DecomposedTransform> decomp = transform.Decompose()) {
      decomposed_transforms_[start_offset] =
          std::make_unique<DecomposedTransform>(*decomp);
    } else {
      return false;
    }
  }
  return true;
}

}  // namespace gfx
