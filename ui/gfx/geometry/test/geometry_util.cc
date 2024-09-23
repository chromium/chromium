// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/test/geometry_util.h"

#include <sstream>
#include <string>

#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/outsets.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/selection_bound.h"

namespace gfx {

namespace {

bool FloatAlmostEqual(float a, float b) {
  // FloatLE is the gtest predicate for less than or almost equal to.
  return ::testing::FloatLE("a", "b", a, b) &&
         ::testing::FloatLE("b", "a", b, a);
}

bool FloatNear(float a, float b, float abs_error) {
  return std::abs(a - b) <= abs_error;
}

template <typename T>
::testing::AssertionResult EqFailure(const char* lhs_expr,
                                     const char* rhs_expr,
                                     const T& lhs,
                                     const T& rhs) {
  return ::testing::AssertionFailure()
         << "Expected equality of these values:\n"
         << lhs_expr << "\n    Which is: " << lhs.ToString() << "\n"
         << rhs_expr << "\n    Which is: " << rhs.ToString();
}

template <typename T>
::testing::AssertionResult NearFailure(const char* lhs_expr,
                                       const char* rhs_expr,
                                       const char* abs_error_expr,
                                       const T& lhs,
                                       const T& rhs,
                                       float abs_error) {
  return ::testing::AssertionFailure()
         << "The difference between these values:\n"
         << lhs_expr << "\n    Which is: " << lhs.ToString() << "\n"
         << rhs_expr << "\n    Which is: " << rhs.ToString() << "\nexceeds "
         << abs_error_expr << "\n    Which is: " << abs_error;
}

struct SkRectToString {
  SkRect r;
  std::string ToString() const {
    return base::StringPrintf("SkRect::MakeLTRB(%g, %g, %g, %g)", r.left(),
                              r.top(), r.right(), r.bottom());
  }
};

}  // namespace

::testing::AssertionResult AssertAxisTransform2dFloatEqual(
    const char* lhs_expr,
    const char* rhs_expr,
    const AxisTransform2d& lhs,
    const AxisTransform2d& rhs) {
  if (FloatAlmostEqual(lhs.scale().x(), rhs.scale().x()) &&
      FloatAlmostEqual(lhs.scale().y(), rhs.scale().y()) &&
      FloatAlmostEqual(lhs.translation().x(), rhs.translation().x()) &&
      FloatAlmostEqual(lhs.translation().y(), rhs.translation().y())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, lhs, rhs);
}

::testing::AssertionResult AssertQuaternionFloatEqual(const char* lhs_expr,
                                                      const char* rhs_expr,
                                                      const Quaternion& lhs,
                                                      const Quaternion& rhs) {
  if (FloatAlmostEqual(lhs.x(), rhs.x()) &&
      FloatAlmostEqual(lhs.y(), rhs.y()) &&
      FloatAlmostEqual(lhs.z(), rhs.z()) &&
      FloatAlmostEqual(lhs.w(), rhs.w())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, lhs, rhs);
}

::testing::AssertionResult AssertQuaternionFloatNear(const char* lhs_expr,
                                                     const char* rhs_expr,
                                                     const char* abs_error_expr,
                                                     const Quaternion& lhs,
                                                     const Quaternion& rhs,
                                                     float abs_error) {
  if (FloatNear(lhs.x(), rhs.x(), abs_error) &&
      FloatNear(lhs.y(), rhs.y(), abs_error) &&
      FloatNear(lhs.z(), rhs.z(), abs_error) &&
      FloatNear(lhs.w(), rhs.w(), abs_error)) {
    return ::testing::AssertionSuccess();
  }
  return NearFailure(lhs_expr, rhs_expr, abs_error_expr, lhs, rhs, abs_error);
}

::testing::AssertionResult AssertDecomposedTransformFloatEqual(
    const char* lhs_expr,
    const char* rhs_expr,
    const DecomposedTransform& lhs,
    const DecomposedTransform& rhs) {
#define CHECK_ARRAY(array)                                                 \
  do {                                                                     \
    for (size_t i = 0; i < std::size(lhs.array); i++) {                    \
      if (!FloatAlmostEqual(lhs.array[i], rhs.array[i])) {                 \
        return EqFailure(lhs_expr, rhs_expr, lhs, rhs)                     \
               << "First difference is at: " << #array << "[" << i << "]"; \
      }                                                                    \
    }                                                                      \
  } while (false)

  CHECK_ARRAY(translate);
  CHECK_ARRAY(scale);
  CHECK_ARRAY(skew);
  CHECK_ARRAY(perspective);
#undef CHECK_ARRAY

  return AssertQuaternionFloatEqual(lhs_expr, rhs_expr, lhs.quaternion,
                                    rhs.quaternion)
         << " In quaternion";
}

::testing::AssertionResult AssertDecomposedTransformFloatNear(
    const char* lhs_expr,
    const char* rhs_expr,
    const char* abs_error_expr,
    const DecomposedTransform& lhs,
    const DecomposedTransform& rhs,
    float abs_error) {
#define CHECK_ARRAY(array)                                                 \
  do {                                                                     \
    for (size_t i = 0; i < std::size(lhs.array); i++) {                    \
      if (!FloatNear(lhs.array[i], rhs.array[i], abs_error)) {             \
        return NearFailure(lhs_expr, rhs_expr, abs_error_expr, lhs, rhs,   \
                           abs_error)                                      \
               << "First difference is at: " << #array << "[" << i << "]"; \
      }                                                                    \
    }                                                                      \
  } while (false)

  CHECK_ARRAY(translate);
  CHECK_ARRAY(scale);
  CHECK_ARRAY(skew);
  CHECK_ARRAY(perspective);
#undef CHECK_ARRAY

  return AssertQuaternionFloatNear(lhs_expr, rhs_expr, abs_error_expr,
                                   lhs.quaternion, rhs.quaternion, abs_error)
         << " In quaternion";
}

::testing::AssertionResult AssertTransformFloatEqual(const char* lhs_expr,
                                                     const char* rhs_expr,
                                                     const Transform& lhs,
                                                     const Transform& rhs) {
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      if (!FloatAlmostEqual(lhs.rc(row, col), rhs.rc(row, col))) {
        return EqFailure(lhs_expr, rhs_expr, lhs, rhs)
               << "\nFirst difference at row: " << row << " col: " << col;
      }
    }
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult AssertTransformFloatNear(const char* lhs_expr,
                                                    const char* rhs_expr,
                                                    const char* abs_error_expr,
                                                    const Transform& lhs,
                                                    const Transform& rhs,
                                                    float abs_error) {
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      if (!FloatNear(lhs.rc(row, col), rhs.rc(row, col), abs_error)) {
        return NearFailure(lhs_expr, rhs_expr, abs_error_expr, lhs, rhs,
                           abs_error)
               << "\nFirst difference at row: " << row << " col: " << col;
      }
    }
  }
  return ::testing::AssertionSuccess();
}

::testing::AssertionResult AssertBoxFloatEqual(const char* lhs_expr,
                                               const char* rhs_expr,
                                               const BoxF& lhs,
                                               const BoxF& rhs) {
  if (FloatAlmostEqual(lhs.x(), rhs.x()) &&
      FloatAlmostEqual(lhs.y(), rhs.y()) &&
      FloatAlmostEqual(lhs.z(), rhs.z()) &&
      FloatAlmostEqual(lhs.width(), rhs.width()) &&
      FloatAlmostEqual(lhs.height(), rhs.height()) &&
      FloatAlmostEqual(lhs.depth(), rhs.depth())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, lhs, rhs);
}

::testing::AssertionResult AssertBoxFloatNear(const char* lhs_expr,
                                              const char* rhs_expr,
                                              const char* abs_error_expr,
                                              const BoxF& lhs,
                                              const BoxF& rhs,
                                              float abs_error) {
  if (FloatNear(lhs.x(), rhs.x(), abs_error) &&
      FloatNear(lhs.y(), rhs.y(), abs_error) &&
      FloatNear(lhs.z(), rhs.z(), abs_error) &&
      FloatNear(lhs.width(), rhs.width(), abs_error) &&
      FloatNear(lhs.height(), rhs.height(), abs_error) &&
      FloatNear(lhs.depth(), rhs.depth(), abs_error)) {
    return ::testing::AssertionSuccess();
  }
  return NearFailure(lhs_expr, rhs_expr, abs_error_expr, lhs, rhs, abs_error);
}

::testing::AssertionResult AssertPointFloatEqual(const char* lhs_expr,
                                                 const char* rhs_expr,
                                                 const PointF& lhs,
                                                 const PointF& rhs) {
  if (FloatAlmostEqual(lhs.x(), rhs.x()) &&
      FloatAlmostEqual(lhs.y(), rhs.y())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, lhs, rhs);
}

::testing::AssertionResult AssertPointFloatNear(const char* lhs_expr,
                                                const char* rhs_expr,
                                                const char* abs_error_expr,
                                                const PointF& lhs,
                                                const PointF& rhs,
                                                float abs_error) {
  if (FloatNear(lhs.x(), rhs.x(), abs_error) &&
      FloatNear(lhs.y(), rhs.y(), abs_error)) {
    return ::testing::AssertionSuccess();
  }
  return NearFailure(lhs_expr, rhs_expr, abs_error_expr, lhs, rhs, abs_error);
}

::testing::AssertionResult AssertPoint3FloatEqual(const char* lhs_expr,
                                                  const char* rhs_expr,
                                                  const Point3F& lhs,
                                                  const Point3F& rhs) {
  if (FloatAlmostEqual(lhs.x(), rhs.x()) &&
      FloatAlmostEqual(lhs.y(), rhs.y()) &&
      FloatAlmostEqual(lhs.z(), rhs.z())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, lhs, rhs);
}

::testing::AssertionResult AssertPoint3FloatNear(const char* lhs_expr,
                                                 const char* rhs_expr,
                                                 const char* abs_error_expr,
                                                 const Point3F& lhs,
                                                 const Point3F& rhs,
                                                 float abs_error) {
  if (FloatNear(lhs.x(), rhs.x(), abs_error) &&
      FloatNear(lhs.y(), rhs.y(), abs_error) &&
      FloatNear(lhs.z(), rhs.z(), abs_error)) {
    return ::testing::AssertionSuccess();
  }
  return NearFailure(lhs_expr, rhs_expr, abs_error_expr, lhs, rhs, abs_error);
}

::testing::AssertionResult AssertVector2dFloatEqual(const char* lhs_expr,
                                                    const char* rhs_expr,
                                                    const Vector2dF& lhs,
                                                    const Vector2dF& rhs) {
  if (FloatAlmostEqual(lhs.x(), rhs.x()) &&
      FloatAlmostEqual(lhs.y(), rhs.y())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, lhs, rhs);
}

::testing::AssertionResult AssertVector2dFloatNear(const char* lhs_expr,
                                                   const char* rhs_expr,
                                                   const char* abs_error_expr,
                                                   const Vector2dF& lhs,
                                                   const Vector2dF& rhs,
                                                   float abs_error) {
  if (FloatNear(lhs.x(), rhs.x(), abs_error) &&
      FloatNear(lhs.y(), rhs.y(), abs_error)) {
    return ::testing::AssertionSuccess();
  }
  return NearFailure(lhs_expr, rhs_expr, abs_error_expr, lhs, rhs, abs_error);
}

::testing::AssertionResult AssertVector3dFloatEqual(const char* lhs_expr,
                                                    const char* rhs_expr,
                                                    const Vector3dF& lhs,
                                                    const Vector3dF& rhs) {
  if (FloatAlmostEqual(lhs.x(), rhs.x()) &&
      FloatAlmostEqual(lhs.y(), rhs.y()) &&
      FloatAlmostEqual(lhs.z(), rhs.z())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, lhs, rhs);
}

::testing::AssertionResult AssertVector3dFloatNear(const char* lhs_expr,
                                                   const char* rhs_expr,
                                                   const char* abs_error_expr,
                                                   const Vector3dF& lhs,
                                                   const Vector3dF& rhs,
                                                   float abs_error) {
  if (FloatNear(lhs.x(), rhs.x(), abs_error) &&
      FloatNear(lhs.y(), rhs.y(), abs_error) &&
      FloatNear(lhs.z(), rhs.z(), abs_error)) {
    return ::testing::AssertionSuccess();
  }
  return NearFailure(lhs_expr, rhs_expr, abs_error_expr, lhs, rhs, abs_error);
}

::testing::AssertionResult AssertRectFloatEqual(const char* lhs_expr,
                                                const char* rhs_expr,
                                                const RectF& lhs,
                                                const RectF& rhs) {
  if (FloatAlmostEqual(lhs.x(), rhs.x()) &&
      FloatAlmostEqual(lhs.y(), rhs.y()) &&
      FloatAlmostEqual(lhs.width(), rhs.width()) &&
      FloatAlmostEqual(lhs.height(), rhs.height())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, lhs, rhs);
}

::testing::AssertionResult AssertRectFloatNear(const char* lhs_expr,
                                               const char* rhs_expr,
                                               const char* abs_error_expr,
                                               const RectF& lhs,
                                               const RectF& rhs,
                                               float abs_error) {
  if (FloatNear(lhs.x(), rhs.x(), abs_error) &&
      FloatNear(lhs.y(), rhs.y(), abs_error) &&
      FloatNear(lhs.width(), rhs.width(), abs_error) &&
      FloatNear(lhs.height(), rhs.height(), abs_error)) {
    return ::testing::AssertionSuccess();
  }
  return NearFailure(lhs_expr, rhs_expr, abs_error_expr, lhs, rhs, abs_error);
}

::testing::AssertionResult AssertSkRectFloatEqual(const char* lhs_expr,
                                                  const char* rhs_expr,
                                                  const SkRect& lhs,
                                                  const SkRect& rhs) {
  if (FloatAlmostEqual(lhs.x(), rhs.x()) &&
      FloatAlmostEqual(lhs.y(), rhs.y()) &&
      FloatAlmostEqual(lhs.right(), rhs.right()) &&
      FloatAlmostEqual(lhs.bottom(), rhs.bottom())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, SkRectToString{lhs},
                   SkRectToString{rhs});
}

::testing::AssertionResult AssertSizeFloatEqual(const char* lhs_expr,
                                                const char* rhs_expr,
                                                const SizeF& lhs,
                                                const SizeF& rhs) {
  if (FloatAlmostEqual(lhs.width(), rhs.width()) &&
      FloatAlmostEqual(lhs.height(), rhs.height())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, lhs, rhs);
}

::testing::AssertionResult AssertSkSizeFloatEqual(const char* lhs_expr,
                                                  const char* rhs_expr,
                                                  const SkSize& lhs,
                                                  const SkSize& rhs) {
  return AssertSizeFloatEqual(lhs_expr, rhs_expr, SkSizeToSizeF(lhs),
                              SkSizeToSizeF(rhs));
}

::testing::AssertionResult AssertInsetsFloatEqual(const char* lhs_expr,
                                                  const char* rhs_expr,
                                                  const InsetsF& lhs,
                                                  const InsetsF& rhs) {
  if (FloatAlmostEqual(lhs.top(), rhs.top()) &&
      FloatAlmostEqual(lhs.right(), rhs.right()) &&
      FloatAlmostEqual(lhs.bottom(), rhs.bottom()) &&
      FloatAlmostEqual(lhs.left(), rhs.left())) {
    return ::testing::AssertionSuccess();
  }
  return EqFailure(lhs_expr, rhs_expr, lhs, rhs);
}

void PrintTo(const AxisTransform2d& transform, ::std::ostream* os) {
  *os << transform.ToString();
}

void PrintTo(const BoxF& box, ::std::ostream* os) {
  *os << box.ToString();
}

void PrintTo(const Point& point, ::std::ostream* os) {
  *os << point.ToString();
}

void PrintTo(const Point3F& point, ::std::ostream* os) {
  *os << point.ToString();
}

void PrintTo(const PointF& point, ::std::ostream* os) {
  *os << point.ToString();
}

void PrintTo(const Insets& input, ::std::ostream* os) {
  *os << input.ToString();
}

void PrintTo(const InsetsF& input, ::std::ostream* os) {
  *os << input.ToString();
}

void PrintTo(const Outsets& input, ::std::ostream* os) {
  *os << input.ToString();
}

void PrintTo(const OutsetsF& input, ::std::ostream* os) {
  *os << input.ToString();
}

void PrintTo(const QuadF& quad, ::std::ostream* os) {
  *os << quad.ToString();
}

void PrintTo(const Rect& rect, ::std::ostream* os) {
  *os << rect.ToString();
}

void PrintTo(const RectF& rect, ::std::ostream* os) {
  *os << rect.ToString();
}

void PrintTo(const Size& size, ::std::ostream* os) {
  *os << size.ToString();
}

void PrintTo(const SizeF& size, ::std::ostream* os) {
  *os << size.ToString();
}

void PrintTo(const Transform& transform, ::std::ostream* os) {
  *os << transform.ToString();
}

void PrintTo(const Vector2d& vector, ::std::ostream* os) {
  *os << vector.ToString();
}

void PrintTo(const Vector2dF& vector, ::std::ostream* os) {
  *os << vector.ToString();
}

void PrintTo(const Vector3dF& vector, ::std::ostream* os) {
  *os << vector.ToString();
}

void PrintTo(const MaskFilterInfo& info, ::std::ostream* os) {
  *os << info.ToString();
}

void PrintTo(const SelectionBound& bound, ::std::ostream* os) {
  *os << bound.ToString();
}

void PrintTo(const SkRect& rect, ::std::ostream* os) {
  *os << SkRectToString{rect}.ToString();
}

void PrintTo(const DecomposedTransform& transform, ::std::ostream* os) {
  *os << transform.ToString();
}

void PrintTo(const Quaternion& quaternion, ::std::ostream* os) {
  *os << quaternion.ToString();
}

}  // namespace gfx
