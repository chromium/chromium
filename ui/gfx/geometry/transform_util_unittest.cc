// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/transform_util.h"

#include <stddef.h>

#include <algorithm>
#include <limits>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace gfx {
namespace {

#define EXPECT_APPROX_EQ(val1, val2) EXPECT_NEAR(val1, val2, 1e-6);

TEST(TransformUtilTest, GetScaleTransform) {
  const Point kAnchor(20, 40);
  const float kScale = 0.5f;

  Transform scale = GetScaleTransform(kAnchor, kScale);

  const int kOffset = 10;
  for (int sign_x = -1; sign_x <= 1; ++sign_x) {
    for (int sign_y = -1; sign_y <= 1; ++sign_y) {
      Point test = scale.MapPoint(Point(kAnchor.x() + sign_x * kOffset,
                                        kAnchor.y() + sign_y * kOffset));

      EXPECT_EQ(Point(kAnchor.x() + sign_x * kOffset * kScale,
                      kAnchor.y() + sign_y * kOffset * kScale),
                test);
    }
  }
}

TEST(TransformUtilTest, TransformAboutPivot) {
  Transform transform;
  transform.Scale(3, 4);
  transform = TransformAboutPivot(PointF(7, 8), transform);

  Point point = transform.MapPoint(Point(0, 0));
  EXPECT_EQ(Point(-14, -24).ToString(), point.ToString());

  point = transform.MapPoint(Point(1, 1));
  EXPECT_EQ(Point(-11, -20).ToString(), point.ToString());
}

TEST(TransformUtilTest, BlendOppositeQuaternions) {
  DecomposedTransform first;
  DecomposedTransform second;
  second.quaternion.set_w(-second.quaternion.w());

  DecomposedTransform result = BlendDecomposedTransforms(first, second, 0.25);

  EXPECT_TRUE(std::isfinite(result.quaternion.x()));
  EXPECT_TRUE(std::isfinite(result.quaternion.y()));
  EXPECT_TRUE(std::isfinite(result.quaternion.z()));
  EXPECT_TRUE(std::isfinite(result.quaternion.w()));

  EXPECT_FALSE(std::isnan(result.quaternion.x()));
  EXPECT_FALSE(std::isnan(result.quaternion.y()));
  EXPECT_FALSE(std::isnan(result.quaternion.z()));
  EXPECT_FALSE(std::isnan(result.quaternion.w()));
}

TEST(TransformUtilTest, AccumulateDecomposedTransforms) {
  DecomposedTransform a{{2.5, -3.25, 4.75},
                        {4.5, -5.25, 6.75},
                        {1.25, -2.5, 3.75},
                        {5, -4, 3, -2},
                        {-5, 6, -7, 8}};
  DecomposedTransform b{
      {-2, 3, 4}, {-4, 5, 6}, {-1, 2, 3}, {6, 7, -8, -9}, {5, 4, -3, -2}};
  DecomposedTransform expected{{0.5, -0.25, 8.75},
                               {-0.5, -1.25, 11.75},
                               {0.25, -0.5, 6.75},
                               {11, 3, -5, -12},
                               {+60, -30, -60, -36}};
  EXPECT_DECOMPOSED_TRANSFORM_EQ(expected,
                                 AccumulateDecomposedTransforms(a, b));
}

TEST(TransformUtilTest, TransformBetweenRects) {
  auto verify = [](const RectF& src_rect, const RectF& dst_rect) {
    const Transform transform = TransformBetweenRects(src_rect, dst_rect);

    // Applies |transform| to calculate the target rectangle from |src_rect|.
    // Notes that |transform| is in |src_rect|'s local coordinates.
    RectF dst_in_parent_coordinates = transform.MapRect(RectF(src_rect.size()));
    dst_in_parent_coordinates.Offset(src_rect.OffsetFromOrigin());

    // Verifies that the target rectangle is expected.
    EXPECT_EQ(dst_rect, dst_in_parent_coordinates);
  };

  std::vector<std::pair<const RectF, const RectF>> test_cases{
      {RectF(0.f, 0.f, 2.f, 3.f), RectF(3.f, 5.f, 4.f, 9.f)},
      {RectF(10.f, 7.f, 2.f, 6.f), RectF(4.f, 2.f, 1.f, 12.f)},
      {RectF(0.f, 0.f, 3.f, 5.f), RectF(0.f, 0.f, 6.f, 2.5f)}};

  for (const auto& test_case : test_cases) {
    verify(test_case.first, test_case.second);
    verify(test_case.second, test_case.first);
  }

  // Tests the case where the destination is an empty rectangle.
  verify(RectF(0.f, 0.f, 3.f, 5.f), RectF());
}

TEST(TransformUtilTest, OrthoProjectionTransform) {
  auto verify = [](float left, float right, float bottom, float top) {
    AxisTransform2d t = OrthoProjectionTransform(left, right, bottom, top);
    if (right == left || top == bottom) {
      EXPECT_EQ(AxisTransform2d(), t);
    } else {
      EXPECT_EQ(PointF(-1, -1), t.MapPoint(PointF(left, bottom)));
      EXPECT_EQ(PointF(1, 1), t.MapPoint(PointF(right, top)));
    }
  };

  verify(0, 0, 0, 0);
  verify(10, 20, 10, 30);
  verify(10, 30, 20, 30);
  verify(0, 0, 10, 20);
  verify(-100, 400, 200, -200);
  verify(-1.5, 4.25, 2.75, -3.75);
}

TEST(TransformUtilTest, WindowTransform) {
  auto verify = [](int x, int y, int width, int height) {
    AxisTransform2d t = WindowTransform(x, y, width, height);
    EXPECT_EQ(PointF(x, y), t.MapPoint(PointF(-1, -1)));
    EXPECT_EQ(PointF(x + width, y + height), t.MapPoint(PointF(1, 1)));
  };

  verify(0, 0, 0, 0);
  verify(10, 20, 0, 30);
  verify(10, 30, 20, 0);
  verify(0, 0, 10, 20);
  verify(-100, -400, 200, 300);
}

TEST(TransformUtilTest, Transform2dScaleComponents) {
  // Values to test quiet NaN, infinity, and a denormal float if they're
  // present; zero otherwise (since for the case this is used for, it
  // should produce the same result).
  const float quiet_NaN_or_zero = std::numeric_limits<float>::has_quiet_NaN
                                      ? std::numeric_limits<float>::quiet_NaN()
                                      : 0;
  const float infinity_or_zero = std::numeric_limits<float>::has_infinity
                                     ? std::numeric_limits<float>::infinity()
                                     : 0;
  const float denorm_min_or_zero =
      (std::numeric_limits<float>::has_denorm == std::denorm_present)
          ? std::numeric_limits<float>::denorm_min()
          : 0;

  const struct {
    Transform transform;
    std::optional<Vector2dF> expected_scale;
  } tests[] = {
      // clang-format off
      // A matrix with only scale and translation.
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, 1),
       Vector2dF(3, 7)},
      // Matrices like the first, but also with various
      // perspective-altering components.
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, -0.5, 1),
       Vector2dF(3, 7)},
      // The result is always non-negative.
      {Transform::RowMajor(3, 0, 0, -23,
                           0, -7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, -0.5, 1),
       Vector2dF(3, 7)},
      // Values are clamped.
      {Transform::RowMajor(std::numeric_limits<double>::max(), 0, 0, -23,
                           0, std::numeric_limits<double>::lowest(), 0, 31,
                           0, 0, 11, 47,
                           0, 0, -0.5f, 1),
       Vector2dF(FloatGeometrySaturationHandler<float>::max(),
                 FloatGeometrySaturationHandler<float>::max())},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0.2f, 0, -0.5f, 1),
       std::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0.2f, -0.2f, -0.5f, 1),
       std::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0.2f, -0.2f, -0.5f, 1),
       std::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, -0.2f, -0.5f, 1),
       std::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, -0.5f, 0.25f),
       Vector2dF(12, 28)},
      // Matrices like the first, but with some types of rotation.
      {Transform::RowMajor(0, 3, 0, -23,
                           7, 0, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, 1),
       Vector2dF(7, 3)},
      {Transform::RowMajor(3, 8, 0, -23,
                           4, 6, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, 1),
       Vector2dF(5, 10)},
      // Combination of rotation and perspective
      {Transform::RowMajor(3, 8, 0, -23,
                           4, 6, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, 0.25f),
       Vector2dF(20, 40)},
      // Error handling cases for final perspective component.
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, 0),
       std::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, quiet_NaN_or_zero),
       std::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, infinity_or_zero),
       std::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, denorm_min_or_zero),
       std::nullopt},
      // clang-format on
  };

  const float fallback = 1.409718f;  // randomly generated in [1,2)

  for (const auto& test : tests) {
    std::optional<Vector2dF> try_result =
        TryComputeTransform2dScaleComponents(test.transform);
    SCOPED_TRACE(test.transform.ToString());
    EXPECT_EQ(try_result, test.expected_scale);
    Vector2dF result =
        ComputeTransform2dScaleComponents(test.transform, fallback);
    if (test.expected_scale) {
      EXPECT_EQ(result, *test.expected_scale);
    } else {
      EXPECT_EQ(result, Vector2dF(fallback, fallback));
    }
  }
}

}  // namespace
}  // namespace gfx
