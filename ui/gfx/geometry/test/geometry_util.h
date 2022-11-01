// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_GEOMETRY_TEST_GEOMETRY_UTIL_H_
#define UI_GFX_GEOMETRY_TEST_GEOMETRY_UTIL_H_

#include <iosfwd>

#include "testing/gtest/include/gtest/gtest.h"

struct SkRect;
struct SkSize;

namespace gfx {

class AxisTransform2d;
class BoxF;
class InsetsF;
class PointF;
class Point3F;
class Quaternion;
class RectF;
class SizeF;
class Transform;
class Vector2dF;
class Vector3dF;
struct DecomposedTransform;

// This file defines gtest macros for floating-point geometry types. The
// difference from EXPECT_EQ is that each floating-point value is checked with
// something equivalent to EXPECT_FLOAT_EQ which can tolerate floating-point
// errors.

// For integer geometry types, or for floating-point geometry types when you
// know there are no floating-point errors, you can just use EXPECT_EQ.

#define EXPECT_AXIS_TRANSFORM2D_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertAxisTransform2dFloatEqual, a, b)

::testing::AssertionResult AssertAxisTransform2dFloatEqual(
    const char* lhs_expr,
    const char* rhs_expr,
    const AxisTransform2d& lhs,
    const AxisTransform2d& rhs);

#define EXPECT_TRANSFORM_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertTransformFloatEqual, a, b)

::testing::AssertionResult AssertTransformFloatEqual(const char* lhs_expr,
                                                     const char* rhs_expr,
                                                     const Transform& lhs,
                                                     const Transform& rhs);

#define EXPECT_TRANSFORM_NEAR(a, b, abs_error) \
  EXPECT_PRED_FORMAT3(::gfx::AssertTransformFloatNear, a, b, abs_error)

::testing::AssertionResult AssertTransformFloatNear(const char* lhs_expr,
                                                    const char* rhs_expr,
                                                    const char* abs_error_expr,
                                                    const Transform& lhs,
                                                    const Transform& rhs,
                                                    float abs_error);

#define EXPECT_QUATERNION_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertQuaternionFloatEqual, a, b)

::testing::AssertionResult AssertQuaternionFloatEqual(const char* lhs_expr,
                                                      const char* rhs_expr,
                                                      const Quaternion& lhs,
                                                      const Quaternion& rhs);

#define EXPECT_QUATERNION_NEAR(a, b, abs_error) \
  EXPECT_PRED_FORMAT3(::gfx::AssertQuaternionFloatNear, a, b, abs_error)

::testing::AssertionResult AssertQuaternionFloatNear(const char* lhs_expr,
                                                     const char* rhs_expr,
                                                     const char* abs_error_expr,
                                                     const Quaternion& lhs,
                                                     const Quaternion& rhs,
                                                     float abs_error);

#define EXPECT_DECOMPOSED_TRANSFORM_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertDecomposedTransformFloatEqual, a, b)

::testing::AssertionResult AssertDecomposedTransformFloatEqual(
    const char* lhs_expr,
    const char* rhs_expr,
    const DecomposedTransform& lhs,
    const DecomposedTransform& rhs);

#define EXPECT_DECOMPOSED_TRANSFORM_NEAR(a, b, abs_error)              \
  EXPECT_PRED_FORMAT2(::gfx::AssertDecomposedTransformFloatNear, a, b, \
                      abs_error)

::testing::AssertionResult AssertDecomposedTransformFloatNear(
    const char* lhs_expr,
    const char* rhs_expr,
    const char* abs_error_expr,
    const DecomposedTransform& lhs,
    const DecomposedTransform& rhs,
    float abs_error);

#define EXPECT_BOXF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertBoxFloatEqual, a, b)

::testing::AssertionResult AssertBoxFloatEqual(const char* lhs_expr,
                                               const char* rhs_expr,
                                               const BoxF& lhs,
                                               const BoxF& rhs);

#define EXPECT_BOXF_NEAR(a, b, abs_error) \
  EXPECT_PRED_FORMAT3(::gfx::AssertBoxFloatNear, a, b, abs_error)

::testing::AssertionResult AssertBoxFloatNear(const char* lhs_expr,
                                              const char* rhs_expr,
                                              const char* abs_error_expr,
                                              const BoxF& lhs,
                                              const BoxF& rhs,
                                              float abs_error);

#define EXPECT_POINTF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertPointFloatEqual, a, b)

::testing::AssertionResult AssertPointFloatEqual(const char* lhs_expr,
                                                 const char* rhs_expr,
                                                 const PointF& lhs,
                                                 const PointF& rhs);

#define EXPECT_POINTF_NEAR(a, b, abs_error) \
  EXPECT_PRED_FORMAT3(::gfx::AssertPointFloatNear, a, b, abs_error)

::testing::AssertionResult AssertPointFloatNear(const char* lhs_expr,
                                                const char* rhs_expr,
                                                const char* abs_error_expr,
                                                const PointF& lhs,
                                                const PointF& rhs,
                                                float abs_error);

#define EXPECT_POINT3F_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertPoint3FloatEqual, a, b)

::testing::AssertionResult AssertPoint3FloatEqual(const char* lhs_expr,
                                                  const char* rhs_expr,
                                                  const Point3F& lhs,
                                                  const Point3F& rhs);

#define EXPECT_POINT3F_NEAR(a, b, abs_error) \
  EXPECT_PRED_FORMAT3(::gfx::AssertPoint3FloatNear, a, b, abs_error)

::testing::AssertionResult AssertPoint3FloatNear(const char* lhs_expr,
                                                 const char* rhs_expr,
                                                 const char* abs_error_expr,
                                                 const Point3F& lhs,
                                                 const Point3F& rhs,
                                                 float abs_error);

#define EXPECT_VECTOR2DF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertVector2dFloatEqual, a, b)

::testing::AssertionResult AssertVector2dFloatEqual(const char* lhs_expr,
                                                    const char* rhs_expr,
                                                    const Vector2dF& lhs,
                                                    const Vector2dF& rhs);

#define EXPECT_VECTOR2DF_NEAR(a, b, abs_error) \
  EXPECT_PRED_FORMAT3(::gfx::AssertVector2dFloatNear, a, b, abs_error)

::testing::AssertionResult AssertVector2dFloatNear(const char* lhs_expr,
                                                   const char* rhs_expr,
                                                   const char* abs_error_expr,
                                                   const Vector2dF& lhs,
                                                   const Vector2dF& rhs,
                                                   float abs_error);

#define EXPECT_VECTOR3DF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertVector3dFloatEqual, a, b)

::testing::AssertionResult AssertVector3dFloatEqual(const char* lhs_expr,
                                                    const char* rhs_expr,
                                                    const Vector3dF& lhs,
                                                    const Vector3dF& rhs);

#define EXPECT_VECTOR3DF_NEAR(a, b, abs_error) \
  EXPECT_PRED_FORMAT3(::gfx::AssertVector3dFloatNear, a, b, abs_error)

::testing::AssertionResult AssertVector3dFloatNear(const char* lhs_expr,
                                                   const char* rhs_expr,
                                                   const char* abs_error_expr,
                                                   const Vector3dF& lhs,
                                                   const Vector3dF& rhs,
                                                   float abs_error);

#define EXPECT_RECTF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertRectFloatEqual, a, b)

::testing::AssertionResult AssertRectFloatEqual(const char* lhs_expr,
                                                const char* rhs_expr,
                                                const RectF& lhs,
                                                const RectF& rhs);

#define EXPECT_RECTF_NEAR(a, b, abs_error) \
  EXPECT_PRED_FORMAT3(::gfx::AssertRectFloatNear, a, b, abs_error)

::testing::AssertionResult AssertRectFloatNear(const char* lhs_expr,
                                               const char* rhs_expr,
                                               const char* abs_error_expr,
                                               const RectF& lhs,
                                               const RectF& rhs,
                                               float abs_error);

#define EXPECT_SKRECT_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertSkRectFloatEqual, a, b)

::testing::AssertionResult AssertSkRectFloatEqual(const char* lhs_expr,
                                                  const char* rhs_expr,
                                                  const SkRect& lhs,
                                                  const SkRect& rhs);

#define EXPECT_SIZEF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertSizeFloatEqual, a, b)

::testing::AssertionResult AssertSizeFloatEqual(const char* lhs_expr,
                                                const char* rhs_expr,
                                                const SizeF& lhs,
                                                const SizeF& rhs);

#define EXPECT_SKSIZE_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertSkSizeFloatEqual, a, b)

::testing::AssertionResult AssertSkSizeFloatEqual(const char* lhs_expr,
                                                  const char* rhs_expr,
                                                  const SkSize& lhs,
                                                  const SkSize& rhs);

#define EXPECT_INSETSF_EQ(a, b) \
  EXPECT_PRED_FORMAT2(::gfx::AssertInsetsFloatEqual, a, b)

::testing::AssertionResult AssertInsetsFloatEqual(const char* lhs_expr,
                                                  const char* rhs_expr,
                                                  const InsetsF& lhs,
                                                  const InsetsF& rhs);

void PrintTo(const SkRect& rect, ::std::ostream* os);

}  // namespace gfx

#endif  // UI_GFX_GEOMETRY_TEST_GEOMETRY_UTIL_H_
