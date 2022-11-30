// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/transform_util.h"

#include <stddef.h>
#include <limits>

#include "base/cxx17_backports.h"
#include "base/numerics/math_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"

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

TEST(TransformUtilTest, SnapRotation) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;
  transform.RotateAboutZAxis(89.99);

  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_TRUE(snapped) << "Viewport should snap for this rotation.";
}

TEST(TransformUtilTest, SnapRotationDistantViewport) {
  const int kOffset = 5000;
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.RotateAboutZAxis(89.99);

  Rect viewport(kOffset, kOffset, 1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_FALSE(snapped) << "Distant viewport shouldn't snap by more than 1px.";
}

TEST(TransformUtilTest, NoSnapRotation) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;
  const int kOffset = 5000;

  transform.RotateAboutZAxis(89.9);

  Rect viewport(kOffset, kOffset, 1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_FALSE(snapped) << "Viewport should not snap for this rotation.";
}

// Translations should always be snappable, the most we would move is 0.5
// pixels towards either direction to the nearest value in each component.
TEST(TransformUtilTest, SnapTranslation) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.Translate3d(SkDoubleToScalar(1.01), SkDoubleToScalar(1.99),
                        SkDoubleToScalar(3.0));

  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_TRUE(snapped) << "Viewport should snap for this translation.";
}

TEST(TransformUtilTest, SnapTranslationDistantViewport) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;
  const int kOffset = 5000;

  transform.Translate3d(SkDoubleToScalar(1.01), SkDoubleToScalar(1.99),
                        SkDoubleToScalar(3.0));

  Rect viewport(kOffset, kOffset, 1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_TRUE(snapped)
      << "Distant viewport should still snap by less than 1px.";
}

TEST(TransformUtilTest, SnapScale) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.Scale3d(SkDoubleToScalar(5.0), SkDoubleToScalar(2.00001),
                    SkDoubleToScalar(1.0));
  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_TRUE(snapped) << "Viewport should snap for this scaling.";
}

TEST(TransformUtilTest, NoSnapScale) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.Scale3d(SkDoubleToScalar(5.0), SkDoubleToScalar(2.1),
                    SkDoubleToScalar(1.0));
  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);

  EXPECT_FALSE(snapped) << "Viewport shouldn't snap for this scaling.";
}

TEST(TransformUtilTest, SnapCompositeTransform) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.Translate3d(SkDoubleToScalar(30.5), SkDoubleToScalar(20.0),
                        SkDoubleToScalar(10.1));
  transform.RotateAboutZAxis(89.99);
  transform.Scale3d(SkDoubleToScalar(1.0), SkDoubleToScalar(3.00001),
                    SkDoubleToScalar(2.0));

  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);
  ASSERT_TRUE(snapped) << "Viewport should snap all components.";

  Point3F point = result.MapPoint(Point3F(PointF(viewport.origin())));
  EXPECT_EQ(Point3F(31.f, 20.f, 10.f), point) << "Transformed origin";

  point = result.MapPoint(Point3F(PointF(viewport.top_right())));
  EXPECT_EQ(Point3F(31.f, 1940.f, 10.f), point) << "Transformed top-right";

  point = result.MapPoint(Point3F(PointF(viewport.bottom_left())));
  EXPECT_EQ(Point3F(-3569.f, 20.f, 10.f), point) << "Transformed bottom-left";

  point = result.MapPoint(Point3F(PointF(viewport.bottom_right())));
  EXPECT_EQ(Point3F(-3569.f, 1940.f, 10.f), point)
      << "Transformed bottom-right";
}

TEST(TransformUtilTest, NoSnapSkewedCompositeTransform) {
  Transform result(Transform::kSkipInitialization);
  Transform transform;

  transform.RotateAboutZAxis(89.99);
  transform.Scale3d(SkDoubleToScalar(1.0), SkDoubleToScalar(3.00001),
                    SkDoubleToScalar(2.0));
  transform.Translate3d(SkDoubleToScalar(30.5), SkDoubleToScalar(20.0),
                        SkDoubleToScalar(10.1));
  transform.Skew(20.0, 0.0);
  Rect viewport(1920, 1200);
  bool snapped = SnapTransform(&result, transform, viewport);
  EXPECT_FALSE(snapped) << "Skewed viewport should not snap.";
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

double ComputeDecompRecompError(const Transform& transform) {
  DecomposedTransform decomp;
  DecomposeTransform(&decomp, transform);
  Transform composed = ComposeTransform(decomp);

  float expected[16];
  float actual[16];
  transform.GetColMajorF(expected);
  composed.GetColMajorF(actual);
  double sse = 0;
  for (int i = 0; i < 16; i++) {
    double diff = expected[i] - actual[i];
    sse += diff * diff;
  }
  return sse;
}

TEST(TransformUtilTest, RoundTripTest) {
  // rotateZ(90deg)
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform::Make90degRotation()));

  // rotateZ(180deg)
  // Edge case where w = 0.
  EXPECT_APPROX_EQ(0,
                   ComputeDecompRecompError(Transform::Make180degRotation()));

  // rotateX(90deg) rotateY(90deg) rotateZ(90deg)
  // [1  0   0][ 0 0 1][0 -1 0]   [0 0 1][0 -1 0]   [0  0 1]
  // [0  0  -1][ 0 1 0][1  0 0] = [1 0 0][1  0 0] = [0 -1 0]
  // [0  1   0][-1 0 0][0  0 1]   [0 1 0][0  0 1]   [1  0 0]
  // This test case leads to Gimbal lock when using Euler angles.
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform::RowMajor(
                          0, 0, 1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1)));

  // Quaternion matrices with 0 off-diagonal elements, and negative trace.
  // Stress tests handling of degenerate cases in computing quaternions.
  // Validates fix for https://crbug.com/647554.
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform::RowMajor(
                          1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)));
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform::MakeScale(-1, 1)));
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(Transform::MakeScale(1, -1)));
  Transform flip_z;
  flip_z.Scale3d(1, 1, -1);
  EXPECT_APPROX_EQ(0, ComputeDecompRecompError(flip_z));
}

TEST(TransformUtilTest, Transform2D) {
  // The spec covering interpolation of 2D matrix transforms calls for inverting
  // one of the axis in the case of a negative determinant.  This differs from
  // the general 3D spec, which calls for flipping all of the scales when the
  // determinant is negative. Flipping all scales not only introduces rotation
  // in the case of a trivial scale inversion, but causes transformed objects
  // to needlessly shrink and grow as they transform through scale = 0 along
  // multiple axes.  2D transformation matrices should follow the 2D spec
  // regarding matrix decomposition.
  DecomposedTransform decomp_flip_x;
  DecomposeTransform(&decomp_flip_x, Transform::MakeScale(-1, 1));
  EXPECT_APPROX_EQ(-1, decomp_flip_x.scale[0]);
  EXPECT_APPROX_EQ(1, decomp_flip_x.scale[1]);
  EXPECT_APPROX_EQ(1, decomp_flip_x.scale[2]);
  EXPECT_APPROX_EQ(0, decomp_flip_x.quaternion.z());
  EXPECT_APPROX_EQ(1, decomp_flip_x.quaternion.w());

  DecomposedTransform decomp_flip_y;
  DecomposeTransform(&decomp_flip_y, Transform::MakeScale(1, -1));
  EXPECT_APPROX_EQ(1, decomp_flip_y.scale[0]);
  EXPECT_APPROX_EQ(-1, decomp_flip_y.scale[1]);
  EXPECT_APPROX_EQ(1, decomp_flip_y.scale[2]);
  EXPECT_APPROX_EQ(0, decomp_flip_y.quaternion.z());
  EXPECT_APPROX_EQ(1, decomp_flip_y.quaternion.w());

  DecomposedTransform decomp_rotate_180;
  DecomposeTransform(&decomp_rotate_180, Transform::Make180degRotation());
  EXPECT_APPROX_EQ(1, decomp_rotate_180.scale[0]);
  EXPECT_APPROX_EQ(1, decomp_rotate_180.scale[1]);
  EXPECT_APPROX_EQ(1, decomp_rotate_180.scale[2]);
  EXPECT_APPROX_EQ(1, decomp_rotate_180.quaternion.z());
  EXPECT_APPROX_EQ(0, decomp_rotate_180.quaternion.w());

  DecomposedTransform decomp_rotate_90;
  DecomposeTransform(&decomp_rotate_90, Transform::Make90degRotation());
  EXPECT_APPROX_EQ(1, decomp_rotate_90.scale[0]);
  EXPECT_APPROX_EQ(1, decomp_rotate_90.scale[1]);
  EXPECT_APPROX_EQ(1, decomp_rotate_90.scale[2]);
  EXPECT_APPROX_EQ(1 / sqrt(2), decomp_rotate_90.quaternion.z());
  EXPECT_APPROX_EQ(1 / sqrt(2), decomp_rotate_90.quaternion.w());

  DecomposedTransform decomp_translate_rotate_90;
  DecomposeTransform(
      &decomp_translate_rotate_90,
      Transform::MakeTranslation(-1, 1) * Transform::Make90degRotation());
  EXPECT_APPROX_EQ(1, decomp_translate_rotate_90.scale[0]);
  EXPECT_APPROX_EQ(1, decomp_translate_rotate_90.scale[1]);
  EXPECT_APPROX_EQ(1, decomp_translate_rotate_90.scale[2]);
  EXPECT_APPROX_EQ(-1, decomp_translate_rotate_90.translate[0]);
  EXPECT_APPROX_EQ(1, decomp_translate_rotate_90.translate[1]);
  EXPECT_APPROX_EQ(0, decomp_translate_rotate_90.translate[2]);
  EXPECT_APPROX_EQ(1 / sqrt(2), decomp_translate_rotate_90.quaternion.z());
  EXPECT_APPROX_EQ(1 / sqrt(2), decomp_translate_rotate_90.quaternion.w());

  DecomposedTransform decomp_skew_rotate;
  DecomposeTransform(
      &decomp_skew_rotate,
      Transform::RowMajor(1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1));
  EXPECT_APPROX_EQ(sqrt(2), decomp_skew_rotate.scale[0]);
  EXPECT_APPROX_EQ(-1 / sqrt(2), decomp_skew_rotate.scale[1]);
  EXPECT_APPROX_EQ(1, decomp_skew_rotate.scale[2]);
  EXPECT_APPROX_EQ(-1, decomp_skew_rotate.skew[0]);
  EXPECT_APPROX_EQ(sin(base::kPiDouble / 8), decomp_skew_rotate.quaternion.z());
  EXPECT_APPROX_EQ(cos(base::kPiDouble / 8), decomp_skew_rotate.quaternion.w());
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
    absl::optional<Vector2dF> expected_scale;
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
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0.2f, 0, -0.5f, 1),
       absl::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0.2f, -0.2f, -0.5f, 1),
       absl::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0.2f, -0.2f, -0.5f, 1),
       absl::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, -0.2f, -0.5f, 1),
       absl::nullopt},
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
       absl::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, quiet_NaN_or_zero),
       absl::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, infinity_or_zero),
       absl::nullopt},
      {Transform::RowMajor(3, 0, 0, -23,
                           0, 7, 0, 31,
                           0, 0, 11, 47,
                           0, 0, 0, denorm_min_or_zero),
       absl::nullopt},
      // clang-format on
  };

  const float fallback = 1.409718f;  // randomly generated in [1,2)

  for (const auto& test : tests) {
    absl::optional<Vector2dF> try_result =
        TryComputeTransform2dScaleComponents(test.transform);
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
