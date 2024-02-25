// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_TRANSFORM_OPERATIONS_H_
#define UI_GFX_GEOMETRY_TRANSFORM_OPERATIONS_H_

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "base/check_op.h"
#include "base/gtest_prod_util.h"
#include "ui/gfx/geometry/geometry_skia_export.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_operation.h"

namespace gfx {

class BoxF;
struct DecomposedTransform;

// Transform operations are a decomposed transformation matrix. It can be
// applied to obtain a Transform at any time, and can be blended
// intelligently with other transform operations, so long as they represent the
// same decomposition. For example, if we have a transform that is made up of
// a rotation followed by skew, it can be blended intelligently with another
// transform made up of a rotation followed by a skew. Blending is possible if
// we have two dissimilar sets of transform operations, but the effect may not
// be what was intended. For more information, see the comments for the blend
// function below.
class GEOMETRY_SKIA_EXPORT TransformOperations {
 public:
  TransformOperations();
  TransformOperations(const TransformOperations& other);
  ~TransformOperations();

  TransformOperations& operator=(const TransformOperations& other);

  // Returns a transformation matrix representing these transform operations.
  Transform Apply() const;

  // Returns a transformation matrix representing the set of transform
  // operations from index |start| to the end of the list.
  Transform ApplyRemaining(size_t start) const;

  // Given another set of transform operations and a progress in the range
  // [0, 1], returns a transformation matrix representing the intermediate
  // value. If this->MatchesTypes(from), then each of the operations are
  // blended separately and then combined. Otherwise, the two sets of
  // transforms are baked to matrices (using apply), and the matrices are
  // then decomposed and interpolated. For more information, see
  // http://www.w3.org/TR/2011/WD-css3-2d-transforms-20111215/#matrix-decomposition.
  //
  // If either of the matrices are non-decomposable for the blend, Blend applies
  // discrete interpolation between them based on the progress value.
  TransformOperations Blend(const TransformOperations& from,
                            SkScalar progress) const;

  // Sets |bounds| be the bounding box for the region within which |box| will
  // exist when it is transformed by the result of calling Blend on |from| and
  // with progress in the range [min_progress, max_progress]. If this region
  // cannot be computed, returns false.
  bool BlendedBoundsForBox(const BoxF& box,
                           const TransformOperations& from,
                           SkScalar min_progress,
                           SkScalar max_progress,
                           BoxF* bounds) const;

  // Returns true if these operations are only translations.
  bool IsTranslation() const;

  // Returns false if the operations affect 2d axis alignment.
  bool PreservesAxisAlignment() const;

  // Returns true if this operation and its descendants have the same types
  // as other and its descendants.
  bool MatchesTypes(const TransformOperations& other) const;

  // Returns the number of matching transform operations at the start of the
  // transform lists. If one list is shorter but pairwise compatible, it will be
  // extended with matching identity operators per spec
  // (https://drafts.csswg.org/css-transforms/#interpolation-of-transforms).
  size_t MatchingPrefixLength(const TransformOperations& other) const;

  // Returns true if these operations can be blended. It will only return
  // false if we must resort to matrix interpolation, and matrix interpolation
  // fails (this can happen if either matrix cannot be decomposed).
  bool CanBlendWith(const TransformOperations& other) const;

  // If none of these operations have a perspective component, sets |scale| to
  // be the product of the scale component of every operation. Otherwise,
  // returns false.
  bool ScaleComponent(SkScalar* scale) const;

  void AppendTranslate(SkScalar x, SkScalar y, SkScalar z);
  void AppendRotate(SkScalar x, SkScalar y, SkScalar z, SkScalar degrees);
  void AppendScale(SkScalar x, SkScalar y, SkScalar z);
  void AppendSkewX(SkScalar x);
  void AppendSkewY(SkScalar y);
  void AppendSkew(SkScalar x, SkScalar y);
  void AppendPerspective(std::optional<SkScalar> depth);
  void AppendMatrix(const Transform& matrix);
  void AppendIdentity();
  void Append(const TransformOperation& operation);
  bool IsIdentity() const;

  size_t size() const { return operations_.size(); }

  const TransformOperation& at(size_t index) const {
    DCHECK_LT(index, size());
    return operations_[index];
  }
  TransformOperation& at(size_t index) {
    DCHECK_LT(index, size());
    return operations_[index];
  }

  bool ApproximatelyEqual(const TransformOperations& other,
                          SkScalar tolerance) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(TransformOperationsTest, TestDecompositionCache);

  bool BlendInternal(const TransformOperations& from,
                     SkScalar progress,
                     TransformOperations* result) const;

  std::vector<TransformOperation> operations_;

  bool ComputeDecomposedTransform(size_t start_offset) const;

  // For efficiency, we cache the decomposed transforms.
  mutable std::unordered_map<size_t, std::unique_ptr<DecomposedTransform>>
      decomposed_transforms_;
};

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_TRANSFORM_OPERATIONS_H_
