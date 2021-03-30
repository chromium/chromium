// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_TRANSFORM_OPERATION_H_
#define UI_GFX_TRANSFORM_OPERATION_H_

#include "ui/gfx/geometry_skia_export.h"
#include "ui/gfx/transform.h"

namespace gfx {
class BoxF;

struct GEOMETRY_SKIA_EXPORT TransformOperation {
  enum Type {
    TRANSFORM_OPERATION_TRANSLATE,
    TRANSFORM_OPERATION_ROTATE,
    TRANSFORM_OPERATION_SCALE,
    TRANSFORM_OPERATION_SKEWX,
    TRANSFORM_OPERATION_SKEWY,
    TRANSFORM_OPERATION_SKEW,
    TRANSFORM_OPERATION_PERSPECTIVE,
    TRANSFORM_OPERATION_MATRIX,
    TRANSFORM_OPERATION_IDENTITY
  };

  TransformOperation() : type(TRANSFORM_OPERATION_IDENTITY) {}

  Type type;
  gfx::Transform matrix;

  union {
    SkScalar perspective_depth;

    struct {
      SkScalar x, y;
    } skew;

    struct {
      SkScalar x, y, z;
    } scale;

    struct {
      SkScalar x, y, z;
    } translate;

    struct {
      struct {
        SkScalar x, y, z;
      } axis;

      SkScalar angle;
    } rotate;
  };

  bool IsIdentity() const;

  // Sets |matrix| based on type and the union values.
  void Bake();

  bool ApproximatelyEqual(const TransformOperation& other,
                          SkScalar tolerance) const;

  static bool BlendTransformOperations(const TransformOperation* from,
                                       const TransformOperation* to,
                                       SkScalar progress,
                                       TransformOperation* result);

  static bool BlendedBoundsForBox(const gfx::BoxF& box,
                                  const TransformOperation* from,
                                  const TransformOperation* to,
                                  SkScalar min_progress,
                                  SkScalar max_progress,
                                  gfx::BoxF* bounds);
};

}  // namespace gfx

#endif  // UI_GFX_TRANSFORM_OPERATION_H_
