// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/transform.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <numbers>
#include <optional>
#include <ostream>

#include "base/numerics/angle_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/box_f.h"
#include "ui/gfx/geometry/decomposed_transform.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {

namespace {

#define STATIC_ROW0_EQ(a, b, c, d, transform) \
  static_assert((a) == (transform).rc(0, 0)); \
  static_assert((b) == (transform).rc(0, 1)); \
  static_assert((c) == (transform).rc(0, 2)); \
  static_assert((d) == (transform).rc(0, 3));

#define STATIC_ROW1_EQ(a, b, c, d, transform) \
  static_assert((a) == (transform).rc(1, 0)); \
  static_assert((b) == (transform).rc(1, 1)); \
  static_assert((c) == (transform).rc(1, 2)); \
  static_assert((d) == (transform).rc(1, 3));

#define STATIC_ROW2_EQ(a, b, c, d, transform) \
  static_assert((a) == (transform).rc(2, 0)); \
  static_assert((b) == (transform).rc(2, 1)); \
  static_assert((c) == (transform).rc(2, 2)); \
  static_assert((d) == (transform).rc(2, 3));

#define STATIC_ROW3_EQ(a, b, c, d, transform) \
  static_assert((a) == (transform).rc(3, 0)); \
  static_assert((b) == (transform).rc(3, 1)); \
  static_assert((c) == (transform).rc(3, 2)); \
  static_assert((d) == (transform).rc(3, 3));

#define EXPECT_ROW0_EQ(a, b, c, d, transform) \
  EXPECT_FLOAT_EQ((a), (transform).rc(0, 0)); \
  EXPECT_FLOAT_EQ((b), (transform).rc(0, 1)); \
  EXPECT_FLOAT_EQ((c), (transform).rc(0, 2)); \
  EXPECT_FLOAT_EQ((d), (transform).rc(0, 3));

#define EXPECT_ROW1_EQ(a, b, c, d, transform) \
  EXPECT_FLOAT_EQ((a), (transform).rc(1, 0)); \
  EXPECT_FLOAT_EQ((b), (transform).rc(1, 1)); \
  EXPECT_FLOAT_EQ((c), (transform).rc(1, 2)); \
  EXPECT_FLOAT_EQ((d), (transform).rc(1, 3));

#define EXPECT_ROW2_EQ(a, b, c, d, transform) \
  EXPECT_FLOAT_EQ((a), (transform).rc(2, 0)); \
  EXPECT_FLOAT_EQ((b), (transform).rc(2, 1)); \
  EXPECT_FLOAT_EQ((c), (transform).rc(2, 2)); \
  EXPECT_FLOAT_EQ((d), (transform).rc(2, 3));

#define EXPECT_ROW3_EQ(a, b, c, d, transform) \
  EXPECT_FLOAT_EQ((a), (transform).rc(3, 0)); \
  EXPECT_FLOAT_EQ((b), (transform).rc(3, 1)); \
  EXPECT_FLOAT_EQ((c), (transform).rc(3, 2)); \
  EXPECT_FLOAT_EQ((d), (transform).rc(3, 3));

// Checking float values for equality close to zero is not robust using
// EXPECT_FLOAT_EQ (see gtest documentation). So, to verify rotation matrices,
// we must use a looser absolute error threshold in some places.
#define EXPECT_ROW0_NEAR(a, b, c, d, transform, errorThreshold) \
  EXPECT_NEAR((a), (transform).rc(0, 0), (errorThreshold));     \
  EXPECT_NEAR((b), (transform).rc(0, 1), (errorThreshold));     \
  EXPECT_NEAR((c), (transform).rc(0, 2), (errorThreshold));     \
  EXPECT_NEAR((d), (transform).rc(0, 3), (errorThreshold));

#define EXPECT_ROW1_NEAR(a, b, c, d, transform, errorThreshold) \
  EXPECT_NEAR((a), (transform).rc(1, 0), (errorThreshold));     \
  EXPECT_NEAR((b), (transform).rc(1, 1), (errorThreshold));     \
  EXPECT_NEAR((c), (transform).rc(1, 2), (errorThreshold));     \
  EXPECT_NEAR((d), (transform).rc(1, 3), (errorThreshold));

#define EXPECT_ROW2_NEAR(a, b, c, d, transform, errorThreshold) \
  EXPECT_NEAR((a), (transform).rc(2, 0), (errorThreshold));     \
  EXPECT_NEAR((b), (transform).rc(2, 1), (errorThreshold));     \
  EXPECT_NEAR((c), (transform).rc(2, 2), (errorThreshold));     \
  EXPECT_NEAR((d), (transform).rc(2, 3), (errorThreshold));

bool PointsAreNearlyEqual(const PointF& lhs, const PointF& rhs) {
  return lhs.IsWithinDistance(rhs, 0.01f);
}

bool PointsAreNearlyEqual(const Point3F& lhs, const Point3F& rhs) {
  return lhs.SquaredDistanceTo(rhs) < 0.0001f;
}

bool MatricesAreNearlyEqual(const Transform& lhs, const Transform& rhs) {
  float epsilon = 0.0001f;
  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 4; ++col) {
      if (std::abs(lhs.rc(row, col) - rhs.rc(row, col)) > epsilon)
        return false;
    }
  }
  return true;
}

Transform GetTestMatrix1() {
  // clang-format off
  constexpr Transform transform = Transform::ColMajor(10.0, 11.0, 12.0, 13.0,
                                                      14.0, 15.0, 16.0, 17.0,
                                                      18.0, 19.0, 20.0, 21.0,
                                                      22.0, 23.0, 24.0, 25.0);
  // clang-format on

  STATIC_ROW0_EQ(10.0, 14.0, 18.0, 22.0, transform);
  STATIC_ROW1_EQ(11.0, 15.0, 19.0, 23.0, transform);
  STATIC_ROW2_EQ(12.0, 16.0, 20.0, 24.0, transform);
  STATIC_ROW3_EQ(13.0, 17.0, 21.0, 25.0, transform);

  EXPECT_ROW0_EQ(10.0, 14.0, 18.0, 22.0, transform);
  EXPECT_ROW1_EQ(11.0, 15.0, 19.0, 23.0, transform);
  EXPECT_ROW2_EQ(12.0, 16.0, 20.0, 24.0, transform);
  EXPECT_ROW3_EQ(13.0, 17.0, 21.0, 25.0, transform);
  return transform;
}

Transform GetTestMatrix2() {
  constexpr Transform transform =
      Transform::RowMajor(30.0, 34.0, 38.0, 42.0, 31.0, 35.0, 39.0, 43.0, 32.0,
                          36.0, 40.0, 44.0, 33.0, 37.0, 41.0, 45.0);
  // clang-format on

  STATIC_ROW0_EQ(30.0, 34.0, 38.0, 42.0, transform);
  STATIC_ROW1_EQ(31.0, 35.0, 39.0, 43.0, transform);
  STATIC_ROW2_EQ(32.0, 36.0, 40.0, 44.0, transform);
  STATIC_ROW3_EQ(33.0, 37.0, 41.0, 45.0, transform);

  EXPECT_ROW0_EQ(30.0, 34.0, 38.0, 42.0, transform);
  EXPECT_ROW1_EQ(31.0, 35.0, 39.0, 43.0, transform);
  EXPECT_ROW2_EQ(32.0, 36.0, 40.0, 44.0, transform);
  EXPECT_ROW3_EQ(33.0, 37.0, 41.0, 45.0, transform);
  return transform;
}

Transform ApproxIdentityMatrix(double error) {
  return Transform::ColMajor(1.0 - error, error, error, error,   // col0
                             error, 1.0 - error, error, error,   // col1
                             error, error, 1.0 - error, error,   // col2
                             error, error, error, 1.0 - error);  // col3
}

constexpr double kErrorThreshold = 1e-7;

// This test is to make it easier to understand the order of operations.
TEST(XFormTest, PrePostOperations) {
  auto m1 = Transform::Affine(1, 2, 3, 4, 5, 6);
  auto m2 = m1;
  m1.Translate(10, 20);
  m2.PreConcat(Transform::MakeTranslation(10, 20));
  EXPECT_EQ(m1, m2);

  m1.PostTranslate(11, 22);
  m2.PostConcat(Transform::MakeTranslation(11, 22));
  EXPECT_EQ(m1, m2);

  m1.Scale(3, 4);
  m2.PreConcat(Transform::MakeScale(3, 4));
  EXPECT_EQ(m1, m2);

  m1.PostScale(5, 6);
  m2.PostConcat(Transform::MakeScale(5, 6));
  EXPECT_EQ(m1, m2);
}

// This test mostly overlaps with other tests, but similar to the above test,
// this test may help understand how accumulated transforms are equivalent to
// multiple mapping operations e.g. MapPoint().
TEST(XFormTest, BasicOperations) {
  // Just some arbitrary matrix that introduces no rounding, and is unlikely
  // to commute with other operations.
  auto m = Transform::ColMajor(2.f, 3.f, 5.f, 0.f, 7.f, 11.f, 13.f, 0.f, 17.f,
                               19.f, 23.f, 0.f, 29.f, 31.f, 37.f, 1.f);

  Point3F p(41.f, 43.f, 47.f);

  EXPECT_EQ(Point3F(1211.f, 1520.f, 1882.f), m.MapPoint(p));

  {
    Transform n;
    n.Scale(2.f);
    EXPECT_EQ(Point3F(82.f, 86.f, 47.f), n.MapPoint(p));

    Transform mn = m;
    mn.Scale(2.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    Transform n;
    n.Scale(2.f, 3.f);
    EXPECT_EQ(Point3F(82.f, 129.f, 47.f), n.MapPoint(p));

    Transform mn = m;
    mn.Scale(2.f, 3.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    Transform n;
    n.Scale3d(2.f, 3.f, 4.f);
    EXPECT_EQ(Point3F(82.f, 129.f, 188.f), n.MapPoint(p));

    Transform mn = m;
    mn.Scale3d(2.f, 3.f, 4.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    Transform n;
    n.Rotate(90.f);
    EXPECT_FLOAT_EQ(0.f, (Point3F(-43.f, 41.f, 47.f) - n.MapPoint(p)).Length());

    Transform mn = m;
    mn.Rotate(90.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    Transform n;
    n.RotateAbout(10.f, 10.f, 10.f, 120.f);
    EXPECT_FLOAT_EQ(0.f, (Point3F(47.f, 41.f, 43.f) - n.MapPoint(p)).Length());

    Transform mn = m;
    mn.RotateAbout(10.f, 10.f, 10.f, 120.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    Transform n;
    n.Translate(5.f, 6.f);
    EXPECT_EQ(Point3F(46.f, 49.f, 47.f), n.MapPoint(p));

    Transform mn = m;
    mn.Translate(5.f, 6.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    Transform n;
    n.Translate3d(5.f, 6.f, 7.f);
    EXPECT_EQ(Point3F(46.f, 49.f, 54.f), n.MapPoint(p));

    Transform mn = m;
    mn.Translate3d(5.f, 6.f, 7.f);
    EXPECT_EQ(mn.MapPoint(p), m.MapPoint(n.MapPoint(p)));
  }

  {
    Transform nm = m;
    nm.PostTranslate(5.f, 6.f);
    EXPECT_EQ(nm.MapPoint(p), m.MapPoint(p) + Vector3dF(5.f, 6.f, 0.f));
  }

  {
    Transform nm = m;
    nm.PostTranslate3d(5.f, 6.f, 7.f);
    EXPECT_EQ(nm.MapPoint(p), m.MapPoint(p) + Vector3dF(5.f, 6.f, 7.f));
  }

  {
    Transform n;
    n.Skew(45.f, -45.f);
    EXPECT_FLOAT_EQ(0.f, (Point3F(84.f, 2.f, 47.f) - n.MapPoint(p)).Length());

    Transform mn = m;
    mn.Skew(45.f, -45.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    Transform n;
    n.SkewX(45.f);
    EXPECT_FLOAT_EQ(0.f, (Point3F(84.f, 43.f, 47.f) - n.MapPoint(p)).Length());

    Transform mn = m;
    mn.SkewX(45.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    Transform n;
    n.SkewY(45.f);
    EXPECT_FLOAT_EQ(0.f, (Point3F(41.f, 84.f, 47.f) - n.MapPoint(p)).Length());

    Transform mn = m;
    mn.SkewY(45.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    Transform n;
    n.ApplyPerspectiveDepth(94.f);
    EXPECT_FLOAT_EQ(0.f, (Point3F(82.f, 86.f, 94.f) - n.MapPoint(p)).Length());

    Transform mn = m;
    mn.ApplyPerspectiveDepth(94.f);
    EXPECT_FLOAT_EQ(0.f, (mn.MapPoint(p) - m.MapPoint(n.MapPoint(p))).Length());
  }

  {
    Transform n = m;
    n.Zoom(2.f);
    Point3F expectation = p;
    expectation.Scale(0.5f, 0.5f, 0.5f);
    expectation = m.MapPoint(expectation);
    expectation.Scale(2.f, 2.f, 2.f);
    EXPECT_EQ(expectation, n.MapPoint(p));
  }
}

TEST(XFormTest, Equality) {
  Transform lhs, interpolated;
  auto rhs = GetTestMatrix1();
  interpolated = lhs;
  for (int i = 0; i <= 100; ++i) {
    for (int row = 0; row < 4; ++row) {
      for (int col = 0; col < 4; ++col) {
        float a = lhs.rc(row, col);
        float b = rhs.rc(row, col);
        float t = i / 100.0f;
        interpolated.set_rc(row, col, a + (b - a) * t);
      }
    }
    if (i == 100) {
      EXPECT_TRUE(rhs == interpolated);
    } else {
      EXPECT_TRUE(rhs != interpolated);
    }
  }
  lhs = Transform();
  rhs = Transform();
  for (int i = 1; i < 100; ++i) {
    lhs.MakeIdentity();
    rhs.MakeIdentity();
    lhs.Translate(i, i);
    rhs.Translate(-i, -i);
    EXPECT_TRUE(lhs != rhs);
    rhs.Translate(2 * i, 2 * i);
    EXPECT_TRUE(lhs == rhs);
  }
}

TEST(XFormTest, ConcatTranslate) {
  static const struct TestCase {
    int x1;
    int y1;
    float tx;
    float ty;
    int x2;
    int y2;
  } test_cases[] = {
      {0, 0, 10.0f, 20.0f, 10, 20},
      {0, 0, -10.0f, -20.0f, 0, 0},
      {0, 0, -10.0f, -20.0f, -10, -20},
      {0, 0, std::numeric_limits<float>::quiet_NaN(),
       std::numeric_limits<float>::quiet_NaN(), 10, 20},
  };

  Transform xform;
  for (const auto& value : test_cases) {
    Transform translation;
    translation.Translate(value.tx, value.ty);
    xform = translation * xform;
    Point3F p1 = xform.MapPoint(Point3F(value.x1, value.y1, 0));
    Point3F p2(value.x2, value.y2, 0);
    if (value.tx == value.tx && value.ty == value.ty) {
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
    }
  }
}

TEST(XFormTest, ConcatScale) {
  static const struct TestCase {
    int before;
    float scale;
    int after;
  } test_cases[] = {{1, 10.0f, 10},
                    {1, .1f, 1},
                    {1, 100.0f, 100},
                    {1, -1.0f, -100},
                    {1, std::numeric_limits<float>::quiet_NaN(), 1}};

  Transform xform;
  for (const auto& value : test_cases) {
    Transform scale;
    scale.Scale(value.scale, value.scale);
    xform = scale * xform;
    Point3F p1 = xform.MapPoint(Point3F(value.before, value.before, 0));
    Point3F p2(value.after, value.after, 0);
    if (value.scale == value.scale) {
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
    }
  }
}

TEST(XFormTest, ConcatRotate) {
  static const struct TestCase {
    int x1;
    int y1;
    float degrees;
    int x2;
    int y2;
  } test_cases[] = {{1, 0, 90.0f, 0, 1},
                    {1, 0, -90.0f, 1, 0},
                    {1, 0, 90.0f, 0, 1},
                    {1, 0, 360.0f, 0, 1},
                    {1, 0, 0.0f, 0, 1},
                    {1, 0, std::numeric_limits<float>::quiet_NaN(), 1, 0}};

  Transform xform;
  for (const auto& value : test_cases) {
    Transform rotation;
    rotation.Rotate(value.degrees);
    xform = rotation * xform;
    Point3F p1 = xform.MapPoint(Point3F(value.x1, value.y1, 0));
    Point3F p2(value.x2, value.y2, 0);
    if (value.degrees == value.degrees) {
      EXPECT_POINT3F_NEAR(p1, p2, 0.0001f);
    }
  }
}

TEST(XFormTest, ConcatSelf) {
  auto a = Transform::ColMajor(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                               16, 17);
  auto expected_a_times_a =
      Transform::ColMajor(132, 146, 160, 174, 260, 290, 320, 350, 388, 434, 480,
                          526, 516, 578, 640, 702);
  a.PreConcat(a);
  EXPECT_EQ(expected_a_times_a, a);

  a = Transform::Affine(2, 3, 4, 5, 6, 7);
  expected_a_times_a = Transform::Affine(16, 21, 28, 37, 46, 60);
  a.PreConcat(a);
  EXPECT_TRUE(a.Is2dTransform());
  EXPECT_EQ(expected_a_times_a, a);
}

TEST(XFormTest, Translate) {
  static const struct TestCase {
    int x1;
    int y1;
    float tx;
    float ty;
    int x2;
    int y2;
  } test_cases[] = {{0, 0, 10.0f, 20.0f, 10, 20},
                    {10, 20, 10.0f, 20.0f, 20, 40},
                    {10, 20, 0.0f, 0.0f, 10, 20},
                    {0, 0, std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::quiet_NaN(), 0, 0}};

  for (const auto& value : test_cases) {
    for (int k = 0; k < 3; ++k) {
      Point3F p0, p1, p2;
      Transform xform;
      switch (k) {
        case 0:
          p1.SetPoint(value.x1, 0, 0);
          p2.SetPoint(value.x2, 0, 0);
          xform.Translate(value.tx, 0.0);
          break;
        case 1:
          p1.SetPoint(0, value.y1, 0);
          p2.SetPoint(0, value.y2, 0);
          xform.Translate(0.0, value.ty);
          break;
        case 2:
          p1.SetPoint(value.x1, value.y1, 0);
          p2.SetPoint(value.x2, value.y2, 0);
          xform.Translate(value.tx, value.ty);
          break;
      }
      p0 = p1;
      p1 = xform.MapPoint(p1);
      if (value.tx == value.tx && value.ty == value.ty) {
        EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
        const std::optional<Point3F> transformed_p1 = xform.InverseMapPoint(p1);
        ASSERT_TRUE(transformed_p1.has_value());
        EXPECT_TRUE(PointsAreNearlyEqual(transformed_p1.value(), p0));
      }
    }
  }
}

TEST(XFormTest, Scale) {
  static const struct TestCase {
    int before;
    float s;
    int after;
  } test_cases[] = {
      {1, 10.0f, 10},
      {1, 1.0f, 1},
      {1, 0.0f, 0},
      {0, 10.0f, 0},
      {1, std::numeric_limits<float>::quiet_NaN(), 0},
  };

  for (const auto& value : test_cases) {
    for (int k = 0; k < 3; ++k) {
      Point3F p0, p1, p2;
      Transform xform;
      switch (k) {
        case 0:
          p1.SetPoint(value.before, 0, 0);
          p2.SetPoint(value.after, 0, 0);
          xform.Scale(value.s, 1.0);
          break;
        case 1:
          p1.SetPoint(0, value.before, 0);
          p2.SetPoint(0, value.after, 0);
          xform.Scale(1.0, value.s);
          break;
        case 2:
          p1.SetPoint(value.before, value.before, 0);
          p2.SetPoint(value.after, value.after, 0);
          xform.Scale(value.s, value.s);
          break;
      }
      p0 = p1;
      p1 = xform.MapPoint(p1);
      if (value.s == value.s) {
        EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
        if (value.s != 0.0f) {
          const std::optional<Point3F> transformed_p1 =
              xform.InverseMapPoint(p1);
          ASSERT_TRUE(transformed_p1.has_value());
          EXPECT_TRUE(PointsAreNearlyEqual(transformed_p1.value(), p0));
        }
      }
    }
  }
}

TEST(XFormTest, SetRotate) {
  static const struct SetRotateCase {
    int x;
    int y;
    float degree;
    int xprime;
    int yprime;
  } set_rotate_cases[] = {{100, 0, 90.0f, 0, 100},
                          {0, 0, 90.0f, 0, 0},
                          {0, 100, 90.0f, -100, 0},
                          {0, 1, -90.0f, 1, 0},
                          {100, 0, 0.0f, 100, 0},
                          {0, 0, 0.0f, 0, 0},
                          {0, 0, std::numeric_limits<float>::quiet_NaN(), 0, 0},
                          {100, 0, 360.0f, 100, 0}};

  for (const auto& value : set_rotate_cases) {
    Point3F p0;
    Point3F p1(value.x, value.y, 0);
    Point3F p2(value.xprime, value.yprime, 0);
    p0 = p1;
    Transform xform;
    xform.Rotate(value.degree);
    // just want to make sure that we don't crash in the case of NaN.
    if (value.degree == value.degree) {
      p1 = xform.MapPoint(p1);
      EXPECT_TRUE(PointsAreNearlyEqual(p1, p2));
      const std::optional<Point3F> transformed_p1 = xform.InverseMapPoint(p1);
      ASSERT_TRUE(transformed_p1.has_value());
      EXPECT_TRUE(PointsAreNearlyEqual(transformed_p1.value(), p0));
    }
  }
}

// 2D tests
TEST(XFormTest, ConcatTranslate2D) {
  static const struct TestCase {
    int x1;
    int y1;
    float tx;
    float ty;
    int x2;
    int y2;
  } test_cases[] = {
      {0, 0, 10.0f, 20.0f, 10, 20},
      {0, 0, -10.0f, -20.0f, 0, 0},
      {0, 0, -10.0f, -20.0f, -10, -20},
  };

  Transform xform;
  for (const auto& value : test_cases) {
    Transform translation;
    translation.Translate(value.tx, value.ty);
    xform = translation * xform;
    Point p1 = xform.MapPoint(Point(value.x1, value.y1));
    Point p2(value.x2, value.y2);
    if (value.tx == value.tx && value.ty == value.ty) {
      EXPECT_EQ(p1.x(), p2.x());
      EXPECT_EQ(p1.y(), p2.y());
    }
  }
}

TEST(XFormTest, ConcatScale2D) {
  static const struct TestCase {
    int before;
    float scale;
    int after;
  } test_cases[] = {
      {1, 10.0f, 10},
      {1, .1f, 1},
      {1, 100.0f, 100},
      {1, -1.0f, -100},
  };

  Transform xform;
  for (const auto& value : test_cases) {
    Transform scale;
    scale.Scale(value.scale, value.scale);
    xform = scale * xform;
    Point p1 = xform.MapPoint(Point(value.before, value.before));
    Point p2(value.after, value.after);
    if (value.scale == value.scale) {
      EXPECT_EQ(p1.x(), p2.x());
      EXPECT_EQ(p1.y(), p2.y());
    }
  }
}

TEST(XFormTest, ConcatRotate2D) {
  static const struct TestCase {
    int x1;
    int y1;
    float degrees;
    int x2;
    int y2;
  } test_cases[] = {
      {1, 0, 90.0f, 0, 1},  {1, 0, -90.0f, 1, 0}, {1, 0, 90.0f, 0, 1},
      {1, 0, 360.0f, 0, 1}, {1, 0, 0.0f, 0, 1},
  };

  Transform xform;
  for (const auto& value : test_cases) {
    Transform rotation;
    rotation.Rotate(value.degrees);
    xform = rotation * xform;
    Point p1 = xform.MapPoint(Point(value.x1, value.y1));
    Point p2(value.x2, value.y2);
    if (value.degrees == value.degrees) {
      EXPECT_EQ(p1.x(), p2.x());
      EXPECT_EQ(p1.y(), p2.y());
    }
  }
}

TEST(XFormTest, SetTranslate2D) {
  static const struct TestCase {
    int x1;
    int y1;
    float tx;
    float ty;
    int x2;
    int y2;
  } test_cases[] = {
      {0, 0, 10.0f, 20.0f, 10, 20},
      {10, 20, 10.0f, 20.0f, 20, 40},
      {10, 20, 0.0f, 0.0f, 10, 20},
  };

  for (const auto& value : test_cases) {
    for (int j = -1; j < 2; ++j) {
      for (int k = 0; k < 3; ++k) {
        float epsilon = 0.0001f;
        Point p0, p1, p2;
        Transform xform;
        switch (k) {
          case 0:
            p1.SetPoint(value.x1, 0);
            p2.SetPoint(value.x2, 0);
            xform.Translate(value.tx + j * epsilon, 0.0);
            break;
          case 1:
            p1.SetPoint(0, value.y1);
            p2.SetPoint(0, value.y2);
            xform.Translate(0.0, value.ty + j * epsilon);
            break;
          case 2:
            p1.SetPoint(value.x1, value.y1);
            p2.SetPoint(value.x2, value.y2);
            xform.Translate(value.tx + j * epsilon, value.ty + j * epsilon);
            break;
        }
        p0 = p1;
        p1 = xform.MapPoint(p1);
        if (value.tx == value.tx && value.ty == value.ty) {
          EXPECT_EQ(p1.x(), p2.x());
          EXPECT_EQ(p1.y(), p2.y());
          const std::optional<Point> transformed_p1 = xform.InverseMapPoint(p1);
          ASSERT_TRUE(transformed_p1.has_value());
          EXPECT_EQ(transformed_p1->x(), p0.x());
          EXPECT_EQ(transformed_p1->y(), p0.y());
        }
      }
    }
  }
}

TEST(XFormTest, SetScale2D) {
  static const struct TestCase {
    int before;
    float s;
    int after;
  } test_cases[] = {
      {1, 10.0f, 10},
      {1, 1.0f, 1},
      {1, 0.0f, 0},
      {0, 10.0f, 0},
  };

  for (const auto& value : test_cases) {
    for (int j = -1; j < 2; ++j) {
      for (int k = 0; k < 3; ++k) {
        float epsilon = 0.0001f;
        Point p0, p1, p2;
        Transform xform;
        switch (k) {
          case 0:
            p1.SetPoint(value.before, 0);
            p2.SetPoint(value.after, 0);
            xform.Scale(value.s + j * epsilon, 1.0);
            break;
          case 1:
            p1.SetPoint(0, value.before);
            p2.SetPoint(0, value.after);
            xform.Scale(1.0, value.s + j * epsilon);
            break;
          case 2:
            p1.SetPoint(value.before, value.before);
            p2.SetPoint(value.after, value.after);
            xform.Scale(value.s + j * epsilon, value.s + j * epsilon);
            break;
        }
        p0 = p1;
        p1 = xform.MapPoint(p1);
        if (value.s == value.s) {
          EXPECT_EQ(p1.x(), p2.x());
          EXPECT_EQ(p1.y(), p2.y());
          if (value.s != 0.0f) {
            const std::optional<Point> transformed_p1 =
                xform.InverseMapPoint(p1);
            ASSERT_TRUE(transformed_p1.has_value());
            EXPECT_EQ(transformed_p1->x(), p0.x());
            EXPECT_EQ(transformed_p1->y(), p0.y());
          }
        }
      }
    }
  }
}

TEST(XFormTest, SetRotate2D) {
  static const struct SetRotateCase {
    int x;
    int y;
    float degree;
    int xprime;
    int yprime;
  } set_rotate_cases[] = {{100, 0, 90.0f, 0, 100},
                          {0, 0, 90.0f, 0, 0},
                          {0, 100, 90.0f, -100, 0},
                          {0, 1, -90.0f, 1, 0},
                          {100, 0, 0.0f, 100, 0},
                          {0, 0, 0.0f, 0, 0},
                          {0, 0, std::numeric_limits<float>::quiet_NaN(), 0, 0},
                          {100, 0, 360.0f, 100, 0}};

  for (const auto& value : set_rotate_cases) {
    for (int j = 1; j >= -1; --j) {
      float epsilon = 0.1f;
      Point pt(value.x, value.y);
      Transform xform;
      // should be invariant to small floating point errors.
      xform.Rotate(value.degree + j * epsilon);
      // just want to make sure that we don't crash in the case of NaN.
      if (value.degree == value.degree) {
        pt = xform.MapPoint(pt);
        EXPECT_EQ(value.xprime, pt.x());
        EXPECT_EQ(value.yprime, pt.y());
        const std::optional<Point> transformed_pt = xform.InverseMapPoint(pt);
        ASSERT_TRUE(transformed_pt.has_value());
        EXPECT_EQ(transformed_pt->x(), value.x);
        EXPECT_EQ(transformed_pt->y(), value.y);
      }
    }
  }
}

TEST(XFormTest, MapPointWithExtremePerspective) {
  Point3F point(1.f, 1.f, 1.f);
  Transform perspective;
  perspective.ApplyPerspectiveDepth(1.f);
  Point3F transformed = perspective.MapPoint(point);
  EXPECT_EQ(point.ToString(), transformed.ToString());

  perspective.MakeIdentity();
  perspective.ApplyPerspectiveDepth(1.1f);
  transformed = perspective.MapPoint(point);
  EXPECT_FLOAT_EQ(11.f, transformed.x());
  EXPECT_FLOAT_EQ(11.f, transformed.y());
  EXPECT_FLOAT_EQ(11.f, transformed.z());
}

TEST(XFormTest, BlendTranslate) {
  Transform from;
  for (int i = -5; i < 15; ++i) {
    Transform to;
    to.Translate3d(1, 1, 1);
    double t = i / 9.0;
    EXPECT_TRUE(to.Blend(from, t));
    EXPECT_FLOAT_EQ(t, to.rc(0, 3));
    EXPECT_FLOAT_EQ(t, to.rc(1, 3));
    EXPECT_FLOAT_EQ(t, to.rc(2, 3));
  }
}

TEST(XFormTest, BlendRotate) {
  Vector3dF axes[] = {Vector3dF(1, 0, 0), Vector3dF(0, 1, 0),
                      Vector3dF(0, 0, 1), Vector3dF(1, 1, 1)};
  Transform from;
  for (const auto& axis : axes) {
    for (int i = -5; i < 15; ++i) {
      Transform to;
      to.RotateAbout(axis, 90);
      double t = i / 9.0;
      EXPECT_TRUE(to.Blend(from, t));

      Transform expected;
      expected.RotateAbout(axis, 90 * t);

      EXPECT_TRUE(MatricesAreNearlyEqual(expected, to));
    }
  }
}

TEST(XFormTest, CanBlend180DegreeRotation) {
  Vector3dF axes[] = {Vector3dF(1, 0, 0), Vector3dF(0, 1, 0),
                      Vector3dF(0, 0, 1), Vector3dF(1, 1, 1)};
  Transform from;
  for (const auto& axis : axes) {
    for (int i = -5; i < 15; ++i) {
      Transform to;
      to.RotateAbout(axis, 180.0);
      double t = i / 9.0;
      EXPECT_TRUE(to.Blend(from, t));

      // A 180 degree rotation is exactly opposite on the sphere, therefore
      // either great circle arc to it is equivalent (and numerical precision
      // will determine which is closer).  Test both directions.
      Transform expected1;
      expected1.RotateAbout(axis, 180.0 * t);
      Transform expected2;
      expected2.RotateAbout(axis, -180.0 * t);

      EXPECT_TRUE(MatricesAreNearlyEqual(expected1, to) ||
                  MatricesAreNearlyEqual(expected2, to))
          << "to: " << to.ToString() << "expected1: " << expected1.ToString()
          << "expected2: " << expected2.ToString()
          << "axis: " << axis.ToString() << ", i: " << i;
    }
  }
}

TEST(XFormTest, BlendScale) {
  Transform from;
  for (int i = -5; i < 15; ++i) {
    Transform to;
    to.Scale3d(5, 4, 3);
    double s1 = i / 9.0;
    double s2 = 1 - s1;
    EXPECT_TRUE(to.Blend(from, s1));
    EXPECT_FLOAT_EQ(5 * s1 + s2, to.rc(0, 0)) << "i: " << i;
    EXPECT_FLOAT_EQ(4 * s1 + s2, to.rc(1, 1)) << "i: " << i;
    EXPECT_FLOAT_EQ(3 * s1 + s2, to.rc(2, 2)) << "i: " << i;
  }
}

TEST(XFormTest, BlendSkew) {
  Transform from;
  for (int i = 0; i < 2; ++i) {
    Transform to;
    to.Skew(10, 5);
    double t = i;
    Transform expected;
    expected.Skew(t * 10, t * 5);
    EXPECT_TRUE(to.Blend(from, t));
    EXPECT_TRUE(MatricesAreNearlyEqual(expected, to))
        << expected.ToString() << "\n"
        << to.ToString();
  }
}

TEST(XFormTest, ExtrapolateSkew) {
  Transform from;
  for (int i = -1; i < 2; ++i) {
    Transform to;
    to.Skew(20, 0);
    double t = i;
    Transform expected;
    expected.Skew(t * 20, t * 0);
    EXPECT_TRUE(to.Blend(from, t));
    EXPECT_TRUE(MatricesAreNearlyEqual(expected, to));
  }
}

TEST(XFormTest, BlendPerspective) {
  Transform from;
  from.ApplyPerspectiveDepth(200);
  for (int i = -1; i < 3; ++i) {
    Transform to;
    to.ApplyPerspectiveDepth(800);
    double t = i;
    double depth = 1.0 / ((1.0 / 200) * (1.0 - t) + (1.0 / 800) * t);
    Transform expected;
    expected.ApplyPerspectiveDepth(depth);
    EXPECT_TRUE(to.Blend(from, t));
    EXPECT_TRUE(MatricesAreNearlyEqual(expected, to));
  }
}

TEST(XFormTest, BlendIdentity) {
  Transform from;
  Transform to;
  EXPECT_TRUE(to.Blend(from, 0.5));
  EXPECT_EQ(to, from);
}

TEST(XFormTest, CannotBlendSingularMatrix) {
  Transform from;
  Transform to;
  to.set_rc(1, 1, 0);
  Transform original_to = to;
  EXPECT_FALSE(to.Blend(from, 0.25));
  EXPECT_EQ(original_to, to);
  EXPECT_FALSE(to.Blend(from, 0.75));
  EXPECT_EQ(original_to, to);
}

TEST(XFormTest, VerifyBlendForTranslation) {
  Transform from;
  from.Translate3d(100.0, 200.0, 100.0);

  Transform to;

  to.Translate3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  to = Transform();
  to.Translate3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.25);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 125.0f, to);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 175.0f, to);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 150.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Translate3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.5);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 150.0f, to);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 150.0f, to);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 200.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Translate3d(200.0, 100.0, 300.0);
  to.Blend(from, 1.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 200.0f, to);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 100.0f, to);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 300.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, VerifyBlendForScale) {
  Transform from;
  from.Scale3d(100.0, 200.0, 100.0);

  Transform to;

  to.Scale3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  to = Transform();
  to.Scale3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.25);
  EXPECT_ROW0_EQ(125.0f, 0.0f, 0.0f, 0.0f, to);
  EXPECT_ROW1_EQ(0.0f, 175.0f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 150.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Scale3d(200.0, 100.0, 300.0);
  to.Blend(from, 0.5);
  EXPECT_ROW0_EQ(150.0f, 0.0f, 0.0f, 0.0f, to);
  EXPECT_ROW1_EQ(0.0f, 150.0f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 200.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Scale3d(200.0, 100.0, 300.0);
  to.Blend(from, 1.0);
  EXPECT_ROW0_EQ(200.0f, 0.0f, 0.0f, 0.0f, to);
  EXPECT_ROW1_EQ(0.0f, 100.0f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 300.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, VerifyBlendForSkew) {
  // Along X axis only
  Transform from;
  from.Skew(0.0, 0.0);

  Transform to;

  to.Skew(45.0, 0.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  to = Transform();
  to.Skew(45.0, 0.0);
  to.Blend(from, 0.5);
  EXPECT_ROW0_EQ(1.0f, 0.5f, 0.0f, 0.0f, to);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Skew(45.0, 0.0);
  to.Blend(from, 0.25);
  EXPECT_ROW0_EQ(1.0f, 0.25f, 0.0f, 0.0f, to);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Skew(45.0, 0.0);
  to.Blend(from, 1.0);
  EXPECT_ROW0_EQ(1.0f, 1.0f, 0.0f, 0.0f, to);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 0.0f, to);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  // NOTE CAREFULLY: Decomposition of skew and rotation terms of the matrix
  // is inherently underconstrained, and so it does not always compute the
  // originally intended skew parameters. The current implementation uses QR
  // decomposition, which decomposes the shear into a rotation + non-uniform
  // scale.
  //
  // It is unlikely that the decomposition implementation will need to change
  // very often, so to get any test coverage, the compromise is to verify the
  // exact matrix that the.Blend() operation produces.
  //
  // This problem also potentially exists for skew along the X axis, but the
  // current QR decomposition implementation just happens to decompose those
  // test matrices intuitively.
  //
  // Unfortunately, this case suffers from uncomfortably large precision
  // error.

  from = Transform();
  from.Skew(0.0, 0.0);

  to = Transform();

  to.Skew(0.0, 45.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  to = Transform();
  to.Skew(0.0, 45.0);
  to.Blend(from, 0.25);
  EXPECT_LT(1.0, to.rc(0, 0));
  EXPECT_GT(1.5, to.rc(0, 0));
  EXPECT_LT(0.0, to.rc(0, 1));
  EXPECT_GT(0.5, to.rc(0, 1));
  EXPECT_FLOAT_EQ(0.0, to.rc(0, 2));
  EXPECT_FLOAT_EQ(0.0, to.rc(0, 3));

  EXPECT_LT(0.0, to.rc(1, 0));
  EXPECT_GT(0.5, to.rc(1, 0));
  EXPECT_LT(0.0, to.rc(1, 1));
  EXPECT_GT(1.0, to.rc(1, 1));
  EXPECT_FLOAT_EQ(0.0, to.rc(1, 2));
  EXPECT_FLOAT_EQ(0.0, to.rc(1, 3));

  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Skew(0.0, 45.0);
  to.Blend(from, 0.5);

  EXPECT_LT(1.0, to.rc(0, 0));
  EXPECT_GT(1.5, to.rc(0, 0));
  EXPECT_LT(0.0, to.rc(0, 1));
  EXPECT_GT(0.5, to.rc(0, 1));
  EXPECT_FLOAT_EQ(0.0, to.rc(0, 2));
  EXPECT_FLOAT_EQ(0.0, to.rc(0, 3));

  EXPECT_LT(0.0, to.rc(1, 0));
  EXPECT_GT(1.0, to.rc(1, 0));
  EXPECT_LT(0.0, to.rc(1, 1));
  EXPECT_GT(1.0, to.rc(1, 1));
  EXPECT_FLOAT_EQ(0.0, to.rc(1, 2));
  EXPECT_FLOAT_EQ(0.0, to.rc(1, 3));

  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.Skew(0.0, 45.0);
  to.Blend(from, 1.0);
  EXPECT_ROW0_NEAR(1.0, 0.0, 0.0, 0.0, to, kErrorThreshold);
  EXPECT_ROW1_NEAR(1.0, 1.0, 0.0, 0.0, to, kErrorThreshold);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, BlendForRotationAboutX) {
  // Even though.Blending uses quaternions, axis-aligned rotations should.
  // Blend the same with quaternions or Euler angles. So we can test
  // rotation.Blending by comparing against manually specified matrices from
  // Euler angles.

  Transform from;
  from.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 0.0);

  Transform to;

  to.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 90.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  double expectedRotationAngle = base::DegToRad(22.5);
  to = Transform();
  to.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 90.0);
  to.Blend(from, 0.25);
  EXPECT_ROW0_EQ(1.0, 0.0, 0.0, 0.0, to);
  EXPECT_ROW1_NEAR(0.0, std::cos(expectedRotationAngle),
                   -std::sin(expectedRotationAngle), 0.0, to, kErrorThreshold);
  EXPECT_ROW2_NEAR(0.0, std::sin(expectedRotationAngle),
                   std::cos(expectedRotationAngle), 0.0, to, kErrorThreshold);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  expectedRotationAngle = base::DegToRad(45.0);
  to = Transform();
  to.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 90.0);
  to.Blend(from, 0.5);
  EXPECT_ROW0_EQ(1.0, 0.0, 0.0, 0.0, to);
  EXPECT_ROW1_NEAR(0.0, std::cos(expectedRotationAngle),
                   -std::sin(expectedRotationAngle), 0.0, to, kErrorThreshold);
  EXPECT_ROW2_NEAR(0.0, std::sin(expectedRotationAngle),
                   std::cos(expectedRotationAngle), 0.0, to, kErrorThreshold);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 90.0);
  to.Blend(from, 1.0);
  EXPECT_ROW0_EQ(1.0, 0.0, 0.0, 0.0, to);
  EXPECT_ROW1_NEAR(0.0, 0.0, -1.0, 0.0, to, kErrorThreshold);
  EXPECT_ROW2_NEAR(0.0, 1.0, 0.0, 0.0, to, kErrorThreshold);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, BlendForRotationAboutY) {
  Transform from;
  from.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 0.0);

  Transform to;

  to.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 90.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  double expectedRotationAngle = base::DegToRad(22.5);
  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 90.0);
  to.Blend(from, 0.25);
  EXPECT_ROW0_NEAR(std::cos(expectedRotationAngle), 0.0,
                   std::sin(expectedRotationAngle), 0.0, to, kErrorThreshold);
  EXPECT_ROW1_EQ(0.0, 1.0, 0.0, 0.0, to);
  EXPECT_ROW2_NEAR(-std::sin(expectedRotationAngle), 0.0,
                   std::cos(expectedRotationAngle), 0.0, to, kErrorThreshold);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  expectedRotationAngle = base::DegToRad(45.0);
  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 90.0);
  to.Blend(from, 0.5);
  EXPECT_ROW0_NEAR(std::cos(expectedRotationAngle), 0.0,
                   std::sin(expectedRotationAngle), 0.0, to, kErrorThreshold);
  EXPECT_ROW1_EQ(0.0, 1.0, 0.0, 0.0, to);
  EXPECT_ROW2_NEAR(-std::sin(expectedRotationAngle), 0.0,
                   std::cos(expectedRotationAngle), 0.0, to, kErrorThreshold);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 90.0);
  to.Blend(from, 1.0);
  EXPECT_ROW0_NEAR(0.0, 0.0, 1.0, 0.0, to, kErrorThreshold);
  EXPECT_ROW1_EQ(0.0, 1.0, 0.0, 0.0, to);
  EXPECT_ROW2_NEAR(-1.0, 0.0, 0.0, 0.0, to, kErrorThreshold);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, BlendForRotationAboutZ) {
  Transform from;
  from.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 0.0);

  Transform to;

  to.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 90.0);
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  double expectedRotationAngle = base::DegToRad(22.5);
  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 90.0);
  to.Blend(from, 0.25);
  EXPECT_ROW0_NEAR(std::cos(expectedRotationAngle),
                   -std::sin(expectedRotationAngle), 0.0, 0.0, to,
                   kErrorThreshold);
  EXPECT_ROW1_NEAR(std::sin(expectedRotationAngle),
                   std::cos(expectedRotationAngle), 0.0, 0.0, to,
                   kErrorThreshold);
  EXPECT_ROW2_EQ(0.0, 0.0, 1.0, 0.0, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  expectedRotationAngle = base::DegToRad(45.0);
  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 90.0);
  to.Blend(from, 0.5);
  EXPECT_ROW0_NEAR(std::cos(expectedRotationAngle),
                   -std::sin(expectedRotationAngle), 0.0, 0.0, to,
                   kErrorThreshold);
  EXPECT_ROW1_NEAR(std::sin(expectedRotationAngle),
                   std::cos(expectedRotationAngle), 0.0, 0.0, to,
                   kErrorThreshold);
  EXPECT_ROW2_EQ(0.0, 0.0, 1.0, 0.0, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);

  to = Transform();
  to.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 90.0);
  to.Blend(from, 1.0);
  EXPECT_ROW0_NEAR(0.0, -1.0, 0.0, 0.0, to, kErrorThreshold);
  EXPECT_ROW1_NEAR(1.0, 0.0, 0.0, 0.0, to, kErrorThreshold);
  EXPECT_ROW2_EQ(0.0, 0.0, 1.0, 0.0, to);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, to);
}

TEST(XFormTest, BlendForCompositeTransform) {
  // Verify that the.Blending was done with a decomposition in correct order
  // by blending a composite transform. Using matrix x vector notation
  // (Ax = b, where x is column vector), the ordering should be:
  // perspective * translation * rotation * skew * scale
  //
  // It is not as important (or meaningful) to check intermediate
  // interpolations; order of operations will be tested well enough by the
  // end cases that are easier to specify.

  Transform from;
  Transform to;

  Transform expected_end_of_animation;
  expected_end_of_animation.ApplyPerspectiveDepth(1.0);
  expected_end_of_animation.Translate3d(10.0, 20.0, 30.0);
  expected_end_of_animation.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 25.0);
  expected_end_of_animation.Skew(0.0, 45.0);
  expected_end_of_animation.Scale3d(6.0, 7.0, 8.0);

  to = expected_end_of_animation;
  to.Blend(from, 0.0);
  EXPECT_EQ(from, to);

  to = expected_end_of_animation;
  // We short circuit if blend is >= 1, so to check the numerics, we will
  // check that we get close to what we expect when we're nearly done
  // interpolating.
  to.Blend(from, .99999f);

  // Recomposing the matrix results in a normalized matrix, so to verify we
  // need to normalize the expectedEndOfAnimation before comparing elements.
  // Normalizing means dividing everything by expectedEndOfAnimation.m44().
  Transform normalized_expected_end_of_animation = expected_end_of_animation;
  Transform normalization_matrix;
  double inv_w = 1.0 / expected_end_of_animation.rc(3, 3);
  normalization_matrix.set_rc(0, 0, inv_w);
  normalization_matrix.set_rc(1, 1, inv_w);
  normalization_matrix.set_rc(2, 2, inv_w);
  normalization_matrix.set_rc(3, 3, inv_w);
  normalized_expected_end_of_animation.PreConcat(normalization_matrix);

  EXPECT_TRUE(MatricesAreNearlyEqual(normalized_expected_end_of_animation, to));
}

TEST(XFormTest, Blend2dXFlip) {
  // Test 2D x-flip (crbug.com/797472).
  auto from = Transform::Affine(1, 0, 0, 1, 100, 150);
  auto to = Transform::Affine(-1, 0, 0, 1, 400, 150);

  EXPECT_TRUE(from.Is2dTransform());
  EXPECT_TRUE(to.Is2dTransform());

  // OK for interpolated transform to be degenerate.
  Transform result = to;
  EXPECT_TRUE(result.Blend(from, 0.5));
  auto expected = Transform::Affine(0, 0, 0, 1, 250, 150);
  EXPECT_TRANSFORM_EQ(expected, result);
}

TEST(XFormTest, Blend2dRotationDirection) {
  // Interpolate taking shorter rotation path.
  auto from =
      Transform::Affine(-0.5, 0.86602575498, -0.86602575498, -0.5, 0, 0);
  auto to = Transform::Affine(-0.5, -0.86602575498, 0.86602575498, -0.5, 0, 0);

  // Expect clockwise Rotation.
  Transform result = to;
  EXPECT_TRUE(result.Blend(from, 0.5));
  auto expected = Transform::Affine(-1, 0, 0, -1, 0, 0);
  EXPECT_TRANSFORM_EQ(expected, result);

  // Reverse from and to.
  // Expect same midpoint with counter-clockwise rotation.
  result = from;
  EXPECT_TRUE(result.Blend(to, 0.5));
  EXPECT_TRANSFORM_EQ(expected, result);
}

gfx::DecomposedTransform GetRotationDecomp(double x,
                                           double y,
                                           double z,
                                           double w) {
  gfx::DecomposedTransform decomp;
  decomp.quaternion = gfx::Quaternion(x, y, z, w);
  return decomp;
}

const double kCos30deg = std::cos(base::DegToRad(30.0));
const double kSin30deg = 0.5;

TEST(XFormTest, QuaternionFromRotationMatrix) {
  // Test rotation around each axis.

  Transform m;
  m.RotateAbout(1, 0, 0, 60);
  std::optional<DecomposedTransform> decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion,
                         gfx::Quaternion(kSin30deg, 0, 0, kCos30deg), 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 1, 0, 60);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion,
                         gfx::Quaternion(0, kSin30deg, 0, kCos30deg), 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 0, 1, 60);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion,
                         gfx::Quaternion(0, 0, kSin30deg, kCos30deg), 1e-6);

  // Test rotation around non-axis aligned vector.

  m.MakeIdentity();
  m.RotateAbout(1, 1, 0, 60);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(
      decomp->quaternion,
      gfx::Quaternion(kSin30deg / std::numbers::sqrt2,
                      kSin30deg / std::numbers::sqrt2, 0, kCos30deg),
      1e-6);

  // Test edge tests.

  // Cases where q_w = 0. In such cases we resort to basing the calculations on
  // the largest diagonal element in the rotation matrix to ensure numerical
  // stability.

  m.MakeIdentity();
  m.RotateAbout(1, 0, 0, 180);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion, gfx::Quaternion(1, 0, 0, 0), 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 1, 0, 180);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion, gfx::Quaternion(0, 1, 0, 0), 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 0, 1, 180);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion, gfx::Quaternion(0, 0, 1, 0), 1e-6);

  // No rotation.

  m.MakeIdentity();
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion, gfx::Quaternion(0, 0, 0, 1), 1e-6);

  m.MakeIdentity();
  m.RotateAbout(0, 0, 1, 360);
  decomp = m.Decompose();
  ASSERT_TRUE(decomp);
  EXPECT_QUATERNION_NEAR(decomp->quaternion, gfx::Quaternion(0, 0, 0, 1), 1e-6);
}

TEST(XFormTest, QuaternionToRotationMatrixTest) {
  // Test rotation about each axis.
  Transform rotate_x_60deg;
  rotate_x_60deg.RotateAboutXAxis(60);
  EXPECT_TRANSFORM_EQ(rotate_x_60deg, Transform::Compose(GetRotationDecomp(
                                          kSin30deg, 0, 0, kCos30deg)));

  Transform rotate_y_60deg;
  rotate_y_60deg.RotateAboutYAxis(60);
  EXPECT_TRANSFORM_EQ(rotate_y_60deg, Transform::Compose(GetRotationDecomp(
                                          0, kSin30deg, 0, kCos30deg)));

  Transform rotate_z_60deg;
  rotate_z_60deg.RotateAboutZAxis(60);
  EXPECT_TRANSFORM_EQ(rotate_z_60deg, Transform::Compose(GetRotationDecomp(
                                          0, 0, kSin30deg, kCos30deg)));

  // Test non-axis aligned rotation
  Transform rotate_xy_60deg;
  rotate_xy_60deg.RotateAbout(1, 1, 0, 60);
  EXPECT_TRANSFORM_EQ(rotate_xy_60deg,
                      Transform::Compose(GetRotationDecomp(
                          kSin30deg / std::numbers::sqrt2,
                          kSin30deg / std::numbers::sqrt2, 0, kCos30deg)));

  // Test 180deg rotation.
  auto rotate_z_180deg = Transform::Affine(-1, 0, 0, -1, 0, 0);
  EXPECT_TRANSFORM_EQ(rotate_z_180deg,
                      Transform::Compose(GetRotationDecomp(0, 0, 1, 0)));
}

TEST(XFormTest, QuaternionInterpolation) {
  // Rotate from identity matrix.
  Transform from_matrix;
  Transform to_matrix;
  to_matrix.RotateAbout(0, 0, 1, 120);
  to_matrix.Blend(from_matrix, 0.5);
  Transform rotate_z_60;
  rotate_z_60.Rotate(60);
  EXPECT_TRANSFORM_EQ(rotate_z_60, to_matrix);

  // Rotate to identity matrix.
  from_matrix.MakeIdentity();
  from_matrix.RotateAbout(0, 0, 1, 120);
  to_matrix.MakeIdentity();
  EXPECT_TRUE(to_matrix.Blend(from_matrix, 0.5));
  EXPECT_TRANSFORM_EQ(rotate_z_60, to_matrix);

  // Interpolation about a common axis of rotation.
  from_matrix.MakeIdentity();
  from_matrix.RotateAbout(1, 1, 0, 45);
  to_matrix.MakeIdentity();
  from_matrix.RotateAbout(1, 1, 0, 135);
  EXPECT_TRUE(to_matrix.Blend(from_matrix, 0.5));
  Transform rotate_xy_90;
  rotate_xy_90.RotateAbout(1, 1, 0, 90);
  EXPECT_TRANSFORM_NEAR(rotate_xy_90, to_matrix, 1e-15);

  // Interpolation without a common axis of rotation.

  from_matrix.MakeIdentity();
  from_matrix.RotateAbout(1, 0, 0, 90);
  to_matrix.MakeIdentity();
  to_matrix.RotateAbout(0, 0, 1, 90);
  EXPECT_TRUE(to_matrix.Blend(from_matrix, 0.5));
  Transform expected;
  expected.RotateAbout(1 / std::numbers::sqrt2, 0, 1 / std::numbers::sqrt2,
                       70.528778372);
  EXPECT_TRANSFORM_EQ(expected, to_matrix);
}

TEST(XFormTest, ComposeIdentity) {
  DecomposedTransform decomp;
  for (int i = 0; i < 3; ++i) {
    EXPECT_EQ(0.0, decomp.translate[i]);
    EXPECT_EQ(1.0, decomp.scale[i]);
    EXPECT_EQ(0.0, decomp.skew[i]);
    EXPECT_EQ(0.0, decomp.perspective[i]);
  }
  EXPECT_EQ(1.0, decomp.perspective[3]);

  EXPECT_EQ(0.0, decomp.quaternion.x());
  EXPECT_EQ(0.0, decomp.quaternion.y());
  EXPECT_EQ(0.0, decomp.quaternion.z());
  EXPECT_EQ(1.0, decomp.quaternion.w());

  EXPECT_TRUE(Transform::Compose(decomp).IsIdentity());
}

TEST(XFormTest, DecomposeTranslateRotateScale) {
  for (int degrees = 0; degrees < 180; ++degrees) {
    // build a transformation matrix.
    gfx::Transform transform;
    transform.Translate(degrees * 2, -degrees * 3);
    transform.Rotate(degrees);
    transform.Scale(degrees + 1, 2 * degrees + 1);

    // factor the matrix
    std::optional<DecomposedTransform> decomp = transform.Decompose();
    EXPECT_TRUE(decomp);
    EXPECT_FLOAT_EQ(decomp->translate[0], degrees * 2);
    EXPECT_FLOAT_EQ(decomp->translate[1], -degrees * 3);
    double rotation =
        base::RadToDeg(std::acos(double{decomp->quaternion.w()}) * 2);
    while (rotation < 0.0)
      rotation += 360.0;
    while (rotation > 360.0)
      rotation -= 360.0;

    const float epsilon = 0.00015f;
    EXPECT_NEAR(rotation, degrees, epsilon);
    EXPECT_NEAR(decomp->scale[0], degrees + 1, epsilon);
    EXPECT_NEAR(decomp->scale[1], 2 * degrees + 1, epsilon);
  }
}

TEST(XFormTest, DecomposeScaleTransform) {
  for (float scale = 0.001f; scale < 2.0f; scale += 0.001f) {
    Transform transform = Transform::MakeScale(scale);

    std::optional<DecomposedTransform> decomp = transform.Decompose();
    EXPECT_TRUE(decomp);

    Transform compose_transform = Transform::Compose(*decomp);
    EXPECT_TRUE(compose_transform.Preserves2dAxisAlignment());
    EXPECT_EQ(transform, compose_transform);
  }
}

TEST(XFormTest, Decompose2d) {
  DecomposedTransform decomp_flip_x = *Transform::MakeScale(-2, 2).Decompose();
  EXPECT_DECOMPOSED_TRANSFORM_EQ(
      (DecomposedTransform{
          {0, 0, 0}, {-2, 2, 1}, {0, 0, 0}, {0, 0, 0, 1}, {0, 0, 0, 1}}),
      decomp_flip_x);

  DecomposedTransform decomp_flip_y = *Transform::MakeScale(2, -2).Decompose();
  EXPECT_DECOMPOSED_TRANSFORM_EQ(
      (DecomposedTransform{
          {0, 0, 0}, {2, -2, 1}, {0, 0, 0}, {0, 0, 0, 1}, {0, 0, 0, 1}}),
      decomp_flip_y);

  DecomposedTransform decomp_rotate_180 =
      *Transform::Make180degRotation().Decompose();
  EXPECT_DECOMPOSED_TRANSFORM_EQ(
      (DecomposedTransform{
          {0, 0, 0}, {1, 1, 1}, {0, 0, 0}, {0, 0, 0, 1}, {0, 0, 1, 0}}),
      decomp_rotate_180);

  DecomposedTransform decomp_rotate_90 =
      *Transform::Make90degRotation().Decompose();
  EXPECT_DECOMPOSED_TRANSFORM_EQ(
      (DecomposedTransform{
          {0, 0, 0},
          {1, 1, 1},
          {0, 0, 0},
          {0, 0, 0, 1},
          {0, 0, 1.0 / std::numbers::sqrt2, 1.0 / std::numbers::sqrt2}}),
      decomp_rotate_90);

  auto translate_rotate_90 =
      Transform::MakeTranslation(-1, 1) * Transform::Make90degRotation();
  DecomposedTransform decomp_translate_rotate_90 =
      *translate_rotate_90.Decompose();
  EXPECT_DECOMPOSED_TRANSFORM_EQ(
      (DecomposedTransform{
          {-1, 1, 0},
          {1, 1, 1},
          {0, 0, 0},
          {0, 0, 0, 1},
          {0, 0, 1.0 / std::numbers::sqrt2, 1.0 / std::numbers::sqrt2}}),
      decomp_translate_rotate_90);

  DecomposedTransform decomp_skew_rotate =
      *Transform::Affine(1, 1, 1, 0, 0, 0).Decompose();
  EXPECT_DECOMPOSED_TRANSFORM_EQ(
      (DecomposedTransform{{0, 0, 0},
                           {std::numbers::sqrt2, -1.0 / std::numbers::sqrt2, 1},
                           {-1, 0, 0},
                           {0, 0, 0, 1},
                           {0, 0, std::sin(std::numbers::pi / 8),
                            std::cos(std::numbers::pi / 8)}}),
      decomp_skew_rotate);
}

double ComputeDecompRecompError(const Transform& transform) {
  DecomposedTransform decomp = *transform.Decompose();
  Transform composed = Transform::Compose(decomp);

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

TEST(XFormTest, DecomposeAndCompose) {
  // rotateZ(90deg)
  EXPECT_NEAR(0, ComputeDecompRecompError(Transform::Make90degRotation()),
              1e-20);

  // rotateZ(180deg)
  // Edge case where w = 0.
  EXPECT_EQ(0, ComputeDecompRecompError(Transform::Make180degRotation()));

  // rotateX(90deg) rotateY(90deg) rotateZ(90deg)
  // [1  0   0][ 0 0 1][0 -1 0]   [0 0 1][0 -1 0]   [0  0 1]
  // [0  0  -1][ 0 1 0][1  0 0] = [1 0 0][1  0 0] = [0 -1 0]
  // [0  1   0][-1 0 0][0  0 1]   [0 1 0][0  0 1]   [1  0 0]
  // This test case leads to Gimbal lock when using Euler angles.
  EXPECT_NEAR(0,
              ComputeDecompRecompError(Transform::RowMajor(
                  0, 0, 1, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1)),
              1e-20);

  // Quaternion matrices with 0 off-diagonal elements, and negative trace.
  // Stress tests handling of degenerate cases in computing quaternions.
  // Validates fix for https://crbug.com/647554.
  EXPECT_EQ(0, ComputeDecompRecompError(Transform::Affine(1, 1, 1, 0, 0, 0)));
  EXPECT_EQ(0, ComputeDecompRecompError(Transform::MakeScale(-1, 1)));
  EXPECT_EQ(0, ComputeDecompRecompError(Transform::MakeScale(1, -1)));
  Transform flip_z;
  flip_z.Scale3d(1, 1, -1);
  EXPECT_EQ(0, ComputeDecompRecompError(flip_z));

  // The following cases exercise the branches Q_xx/yy/zz for quaternion in
  // Matrix44::Decompose().
  auto transform = [](double sx, double sy, double sz, int skew_r, int skew_c) {
    Transform t;
    t.Scale3d(sx, sy, sz);
    t.set_rc(skew_r, skew_c, 1);
    t.set_rc(skew_c, skew_r, 1);
    return t;
  };
  EXPECT_EQ(0, ComputeDecompRecompError(transform(1, -1, -1, 0, 1)));
  EXPECT_EQ(0, ComputeDecompRecompError(transform(1, -1, -1, 0, 2)));
  EXPECT_EQ(0, ComputeDecompRecompError(transform(-1, 1, -1, 0, 1)));
  EXPECT_EQ(0, ComputeDecompRecompError(transform(-1, 1, -1, 1, 2)));
  EXPECT_EQ(0, ComputeDecompRecompError(transform(-1, -1, 1, 0, 2)));
  EXPECT_EQ(0, ComputeDecompRecompError(transform(-1, -1, 1, 1, 2)));
}

TEST(XFormTest, IsIdentityOr2dTranslation) {
  EXPECT_TRUE(Transform().IsIdentityOr2dTranslation());
  EXPECT_TRUE(Transform::MakeTranslation(10, 0).IsIdentityOr2dTranslation());
  EXPECT_TRUE(Transform::MakeTranslation(0, -20).IsIdentityOr2dTranslation());

  Transform transform;
  transform.Translate3d(0, 0, 1);
  EXPECT_FALSE(transform.IsIdentityOr2dTranslation());

  transform.MakeIdentity();
  transform.Rotate(40);
  EXPECT_FALSE(transform.IsIdentityOr2dTranslation());

  transform.MakeIdentity();
  transform.SkewX(30);
  EXPECT_FALSE(transform.IsIdentityOr2dTranslation());
}

TEST(XFormTest, IntegerTranslation) {
  gfx::Transform transform;
  EXPECT_TRUE(transform.IsIdentityOrIntegerTranslation());

  transform.Translate3d(1, 2, 3);
  EXPECT_TRUE(transform.IsIdentityOrIntegerTranslation());

  transform.MakeIdentity();
  transform.Translate3d(-1, -2, -3);
  EXPECT_TRUE(transform.IsIdentityOrIntegerTranslation());

  transform.MakeIdentity();
  transform.Translate3d(4.5f, 0, 0);
  EXPECT_FALSE(transform.IsIdentityOrIntegerTranslation());

  transform.MakeIdentity();
  transform.Translate3d(0, -6.7f, 0);
  EXPECT_FALSE(transform.IsIdentityOrIntegerTranslation());

  transform.MakeIdentity();
  transform.Translate3d(0, 0, 8.9f);
  EXPECT_FALSE(transform.IsIdentityOrIntegerTranslation());

  float max_int = static_cast<float>(std::numeric_limits<int>::max());
  transform.MakeIdentity();
  transform.Translate3d(0, 0, max_int + 1000.5f);
  EXPECT_FALSE(transform.IsIdentityOrIntegerTranslation());

  float max_float = std::numeric_limits<float>::max();
  transform.MakeIdentity();
  transform.Translate3d(0, 0, max_float - 0.5f);
  EXPECT_FALSE(transform.IsIdentityOrIntegerTranslation());
}

TEST(XFormTest, Integer2dTranslation) {
  EXPECT_TRUE(Transform().IsIdentityOrInteger2dTranslation());
  EXPECT_TRUE(
      Transform::MakeTranslation(1, 2).IsIdentityOrInteger2dTranslation());
  EXPECT_FALSE(Transform::MakeTranslation(1.00001, 2)
                   .IsIdentityOrInteger2dTranslation());
  EXPECT_FALSE(Transform::MakeTranslation(1, 2.00002)
                   .IsIdentityOrInteger2dTranslation());
  EXPECT_FALSE(
      Transform::Make90degRotation().IsIdentityOrInteger2dTranslation());
  Transform transform;
  transform.Translate3d(1, 2, 3);
  EXPECT_FALSE(transform.IsIdentityOrInteger2dTranslation());
}

TEST(XFormTest, Inverse) {
  {
    Transform identity;
    Transform inverse_identity;
    EXPECT_TRUE(identity.GetInverse(&inverse_identity));
    EXPECT_EQ(identity, inverse_identity);
    EXPECT_EQ(identity, identity.InverseOrIdentity());
  }

  {
    // Invert a translation
    Transform translation;
    translation.Translate3d(2.0, 3.0, 4.0);
    EXPECT_TRUE(translation.IsInvertible());

    Transform inverse_translation;
    bool is_invertible = translation.GetInverse(&inverse_translation);
    EXPECT_TRUE(is_invertible);
    EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, -2.0f, inverse_translation);
    EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, -3.0f, inverse_translation);
    EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, -4.0f, inverse_translation);
    EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, inverse_translation);

    EXPECT_EQ(inverse_translation, translation.InverseOrIdentity());

    // GetInverse with the parameter pointing to itself.
    EXPECT_TRUE(translation.GetInverse(&translation));
    EXPECT_EQ(translation, inverse_translation);
  }

  {
    // Invert a non-uniform scale
    Transform scale;
    scale.Scale3d(4.0, 10.0, 100.0);
    EXPECT_TRUE(scale.IsInvertible());

    Transform inverse_scale;
    bool is_invertible = scale.GetInverse(&inverse_scale);
    EXPECT_TRUE(is_invertible);
    EXPECT_ROW0_EQ(0.25f, 0.0f, 0.0f, 0.0f, inverse_scale);
    EXPECT_ROW1_EQ(0.0f, 0.1f, 0.0f, 0.0f, inverse_scale);
    EXPECT_ROW2_EQ(0.0f, 0.0f, 0.01f, 0.0f, inverse_scale);
    EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, inverse_scale);

    EXPECT_EQ(inverse_scale, scale.InverseOrIdentity());
  }

  {
    Transform m1;
    m1.Translate(10, 20);
    m1.Rotate(30);
    Transform m2;
    m2.Rotate(-30);
    m2.Translate(-10, -20);
    Transform inverse_m1, inverse_m2;
    EXPECT_TRUE(m1.GetInverse(&inverse_m1));
    EXPECT_TRUE(m2.GetInverse(&inverse_m2));
    EXPECT_TRUE(inverse_m1.Is2dTransform());
    EXPECT_TRUE(inverse_m2.Is2dTransform());
    EXPECT_TRANSFORM_NEAR(m1, inverse_m2, 1e-6);
    EXPECT_TRANSFORM_NEAR(m2, inverse_m1, 1e-6);
  }

  {
    Transform m1;
    m1.RotateAboutZAxis(-30);
    m1.RotateAboutYAxis(10);
    m1.RotateAboutXAxis(20);
    m1.ApplyPerspectiveDepth(100);
    Transform m2;
    m2.ApplyPerspectiveDepth(-100);
    m2.RotateAboutXAxis(-20);
    m2.RotateAboutYAxis(-10);
    m2.RotateAboutZAxis(30);
    Transform inverse_m1, inverse_m2;
    EXPECT_TRUE(m1.GetInverse(&inverse_m1));
    EXPECT_TRUE(m2.GetInverse(&inverse_m2));
    EXPECT_TRANSFORM_NEAR(m1, inverse_m2, 1e-6);
    EXPECT_TRANSFORM_NEAR(m2, inverse_m1, 1e-6);
  }

  {
    // Try to invert a matrix that is not invertible.
    // The inverse() function should reset the output matrix to identity.
    Transform uninvertible;
    uninvertible.set_rc(0, 0, 0.f);
    uninvertible.set_rc(1, 1, 0.f);
    uninvertible.set_rc(2, 2, 0.f);
    uninvertible.set_rc(3, 3, 0.f);
    EXPECT_FALSE(uninvertible.IsInvertible());

    Transform inverse_of_uninvertible;

    // Add a scale just to more easily ensure that inverse_of_uninvertible is
    // reset to identity.
    inverse_of_uninvertible.Scale3d(4.0, 10.0, 100.0);

    bool is_invertible = uninvertible.GetInverse(&inverse_of_uninvertible);
    EXPECT_FALSE(is_invertible);
    EXPECT_TRUE(inverse_of_uninvertible.IsIdentity());
    EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 0.0f, inverse_of_uninvertible);
    EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 0.0f, inverse_of_uninvertible);
    EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, inverse_of_uninvertible);
    EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, inverse_of_uninvertible);

    EXPECT_EQ(inverse_of_uninvertible, uninvertible.InverseOrIdentity());
  }
}

TEST(XFormTest, verifyBackfaceVisibilityBasicCases) {
  Transform transform;

  transform.MakeIdentity();
  EXPECT_FALSE(transform.IsBackFaceVisible());

  transform.MakeIdentity();
  transform.RotateAboutYAxis(80.0);
  EXPECT_FALSE(transform.IsBackFaceVisible());

  transform.MakeIdentity();
  transform.RotateAboutYAxis(100.0);
  EXPECT_TRUE(transform.IsBackFaceVisible());

  // Edge case, 90 degree rotation should return false.
  transform.MakeIdentity();
  transform.RotateAboutYAxis(90.0);
  EXPECT_FALSE(transform.IsBackFaceVisible());

  // 2d scale doesn't affect backface visibility.
  auto check_scale = [&](float scale_x, float scale_y) {
    transform = Transform::MakeScale(scale_x, scale_y);
    EXPECT_FALSE(transform.IsBackFaceVisible());
    transform.EnsureFullMatrixForTesting();
    EXPECT_FALSE(transform.IsBackFaceVisible());
  };
  check_scale(1, 2);
  check_scale(-1, 2);
  check_scale(1, -2);
  check_scale(-1, -2);
}

TEST(XFormTest, verifyBackfaceVisibilityForPerspective) {
  Transform layer_space_to_projection_plane;

  // This tests if IsBackFaceVisible works properly under perspective
  // transforms.  Specifically, layers that may have their back face visible in
  // orthographic projection, may not actually have back face visible under
  // perspective projection.

  // Case 1: Layer is rotated by slightly more than 90 degrees, at the center
  //         of the perspective projection. In this case, the layer's back-side
  //         is visible to the camera.
  layer_space_to_projection_plane.MakeIdentity();
  layer_space_to_projection_plane.ApplyPerspectiveDepth(1.0);
  layer_space_to_projection_plane.Translate3d(0.0, 0.0, 0.0);
  layer_space_to_projection_plane.RotateAboutYAxis(100.0);
  EXPECT_TRUE(layer_space_to_projection_plane.IsBackFaceVisible());

  // Case 2: Layer is rotated by slightly more than 90 degrees, but shifted off
  //         to the side of the camera. Because of the wide field-of-view, the
  //         layer's front side is still visible.
  //
  //                       |<-- front side of layer is visible to camera
  //                    \  |            /
  //                     \ |           /
  //                      \|          /
  //                       |         /
  //                       |\       /<-- camera field of view
  //                       | \     /
  // back side of layer -->|  \   /
  //                           \./ <-- camera origin
  //
  layer_space_to_projection_plane.MakeIdentity();
  layer_space_to_projection_plane.ApplyPerspectiveDepth(1.0);
  layer_space_to_projection_plane.Translate3d(-10.0, 0.0, 0.0);
  layer_space_to_projection_plane.RotateAboutYAxis(100.0);
  EXPECT_FALSE(layer_space_to_projection_plane.IsBackFaceVisible());

  // Case 3: Additionally rotating the layer by 180 degrees should of course
  //         show the opposite result of case 2.
  layer_space_to_projection_plane.RotateAboutYAxis(180.0);
  EXPECT_TRUE(layer_space_to_projection_plane.IsBackFaceVisible());
}

TEST(XFormTest, verifyDefaultConstructorCreatesIdentityMatrix) {
  constexpr Transform A;
  STATIC_ROW0_EQ(1.0, 0.0, 0.0, 0.0, A);
  STATIC_ROW1_EQ(0.0, 1.0, 0.0, 0.0, A);
  STATIC_ROW2_EQ(0.0, 0.0, 1.0, 0.0, A);
  STATIC_ROW3_EQ(0.0, 0.0, 0.0, 1.0, A);
  EXPECT_TRUE(A.IsIdentity());
}

TEST(XFormTest, verifyCopyConstructor) {
  Transform A = GetTestMatrix1();

  // Copy constructor should produce exact same elements as matrix A.
  Transform B(A);
  EXPECT_EQ(A, B);
  EXPECT_ROW0_EQ(10.0, 14.0, 18.0, 22.0, B);
  EXPECT_ROW1_EQ(11.0, 15.0, 19.0, 23.0, B);
  EXPECT_ROW2_EQ(12.0, 16.0, 20.0, 24.0, B);
  EXPECT_ROW3_EQ(13.0, 17.0, 21.0, 25.0, B);
}

// ColMajor() and RowMajor() are tested in GetTestMatrix1() and
// GetTestTransform2().

TEST(XFormTest, GetColMajor) {
  auto transform = GetTestMatrix1();

  double data[16];
  transform.GetColMajor(data);
  for (int i = 0; i < 16; i++) {
    EXPECT_EQ(i + 10.0, data[i]);
    EXPECT_EQ(data[i], transform.ColMajorData(i));
  }
  EXPECT_EQ(transform, Transform::ColMajor(data));
}

TEST(XFormTest, Affine) {
  constexpr auto transform = Transform::Affine(2.0, 3., 4.0, 5.0, 6.0, 7.0);
  STATIC_ROW0_EQ(2.0, 4.0, 0.0, 6.0, transform);
  STATIC_ROW1_EQ(3.0, 5.0, 0.0, 7.0, transform);
  STATIC_ROW2_EQ(0.0, 0.0, 1.0, 0.0, transform);
  STATIC_ROW3_EQ(0.0, 0.0, 0.0, 1.0, transform);
}

TEST(XFormTest, MakeTranslation) {
  constexpr auto t = Transform::MakeTranslation(3.5, 4.75);
  STATIC_ROW0_EQ(1.0, 0.0, 0.0, 3.5, t);
  STATIC_ROW1_EQ(0.0, 1.0, 0.0, 4.75, t);
  STATIC_ROW2_EQ(0.0, 0.0, 1.0, 0.0, t);
  STATIC_ROW3_EQ(0.0, 0.0, 0.0, 1.0, t);
}

TEST(XFormTest, MakeScale) {
  constexpr auto s = Transform::MakeScale(3.5, 4.75);
  STATIC_ROW0_EQ(3.5, 0.0, 0.0, 0, s);
  STATIC_ROW1_EQ(0.0, 4.75, 0.0, 0, s);
  STATIC_ROW2_EQ(0.0, 0.0, 1.0, 0.0, s);
  STATIC_ROW3_EQ(0.0, 0.0, 0.0, 1.0, s);
}

TEST(XFormTest, MakeRotation) {
  constexpr auto r1 = Transform::Make90degRotation();
  STATIC_ROW0_EQ(0.0, -1.0, 0.0, 0, r1);
  STATIC_ROW1_EQ(1.0, 0.0, 0.0, 0, r1);
  STATIC_ROW2_EQ(0.0, 0.0, 1.0, 0.0, r1);
  STATIC_ROW3_EQ(0.0, 0.0, 0.0, 1.0, r1);

  constexpr auto r2 = Transform::Make180degRotation();
  STATIC_ROW0_EQ(-1.0, 0.0, 0.0, 0, r2);
  STATIC_ROW1_EQ(0.0, -1.0, 0.0, 0, r2);
  STATIC_ROW2_EQ(0.0, 0.0, 1.0, 0.0, r2);
  STATIC_ROW3_EQ(0.0, 0.0, 0.0, 1.0, r2);

  constexpr auto r3 = Transform::Make270degRotation();
  STATIC_ROW0_EQ(0.0, 1.0, 0.0, 0, r3);
  STATIC_ROW1_EQ(-1.0, 0.0, 0.0, 0, r3);
  STATIC_ROW2_EQ(0.0, 0.0, 1.0, 0.0, r3);
  STATIC_ROW3_EQ(0.0, 0.0, 0.0, 1.0, r3);
}

TEST(XFormTest, ColMajorF) {
  float data[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
  auto transform = Transform::ColMajorF(data);

  EXPECT_ROW0_EQ(2.0, 6.0, 10.0, 14.0, transform);
  EXPECT_ROW1_EQ(3.0, 7.0, 11.0, 15.0, transform);
  EXPECT_ROW2_EQ(4.0, 8.0, 12.0, 16.0, transform);
  EXPECT_ROW3_EQ(5.0, 9.0, 13.0, 17.0, transform);

  float data1[16];
  transform.GetColMajorF(data1);
  for (int i = 0; i < 16; i++)
    EXPECT_EQ(data1[i], data[i]);
  EXPECT_EQ(transform, Transform::ColMajorF(data1));
}

TEST(XFormTest, FromQuaternion) {
  Transform t(Quaternion(1, 2, 3, 4));
  EXPECT_ROW0_EQ(-25.f, -20.f, 22.f, 0.f, t);
  EXPECT_ROW1_EQ(28.f, -19.f, 4.f, 0.f, t);
  EXPECT_ROW2_EQ(-10.f, 20.f, -9.f, 0.f, t);
  EXPECT_ROW3_EQ(0.f, 0.f, 0.f, 1.f, t);
}

TEST(XFormTest, verifyAssignmentOperator) {
  Transform A = GetTestMatrix1();
  Transform B = GetTestMatrix2();
  Transform C = GetTestMatrix2();
  C = B = A;

  // Both B and C should now have been re-assigned to the value of A.
  EXPECT_ROW0_EQ(10.0f, 14.0f, 18.0f, 22.0f, B);
  EXPECT_ROW1_EQ(11.0f, 15.0f, 19.0f, 23.0f, B);
  EXPECT_ROW2_EQ(12.0f, 16.0f, 20.0f, 24.0f, B);
  EXPECT_ROW3_EQ(13.0f, 17.0f, 21.0f, 25.0f, B);

  EXPECT_ROW0_EQ(10.0f, 14.0f, 18.0f, 22.0f, C);
  EXPECT_ROW1_EQ(11.0f, 15.0f, 19.0f, 23.0f, C);
  EXPECT_ROW2_EQ(12.0f, 16.0f, 20.0f, 24.0f, C);
  EXPECT_ROW3_EQ(13.0f, 17.0f, 21.0f, 25.0f, C);
}

TEST(XFormTest, verifyEqualsBooleanOperator) {
  Transform A = GetTestMatrix1();
  Transform B = GetTestMatrix1();
  EXPECT_TRUE(A == B);

  // Modifying multiple elements should cause equals operator to return false.
  Transform C = GetTestMatrix2();
  EXPECT_FALSE(A == C);

  // Modifying any one individual element should cause equals operator to
  // return false.
  Transform D;
  D = A;
  D.set_rc(0, 0, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(1, 0, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(2, 0, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(3, 0, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(0, 1, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(1, 1, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(2, 1, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(3, 1, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(0, 2, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(1, 2, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(2, 2, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(3, 2, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(0, 3, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(1, 3, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(2, 3, 0.f);
  EXPECT_FALSE(A == D);

  D = A;
  D.set_rc(3, 3, 0.f);
  EXPECT_FALSE(A == D);
}

TEST(XFormTest, verifyMultiplyOperator) {
  Transform A = GetTestMatrix1();
  Transform B = GetTestMatrix2();

  Transform C = A * B;
  EXPECT_ROW0_EQ(2036.0f, 2292.0f, 2548.0f, 2804.0f, C);
  EXPECT_ROW1_EQ(2162.0f, 2434.0f, 2706.0f, 2978.0f, C);
  EXPECT_ROW2_EQ(2288.0f, 2576.0f, 2864.0f, 3152.0f, C);
  EXPECT_ROW3_EQ(2414.0f, 2718.0f, 3022.0f, 3326.0f, C);

  // Just an additional sanity check; matrix multiplication is not commutative.
  EXPECT_FALSE(A * B == B * A);
}

TEST(XFormTest, verifyMultiplyAndAssignOperator) {
  Transform A = GetTestMatrix1();
  Transform B = GetTestMatrix2();

  A *= B;
  EXPECT_ROW0_EQ(2036.0f, 2292.0f, 2548.0f, 2804.0f, A);
  EXPECT_ROW1_EQ(2162.0f, 2434.0f, 2706.0f, 2978.0f, A);
  EXPECT_ROW2_EQ(2288.0f, 2576.0f, 2864.0f, 3152.0f, A);
  EXPECT_ROW3_EQ(2414.0f, 2718.0f, 3022.0f, 3326.0f, A);

  // Just an additional sanity check; matrix multiplication is not commutative.
  Transform C = A;
  C *= B;
  Transform D = B;
  D *= A;
  EXPECT_FALSE(C == D);
}

TEST(XFormTest, PreConcat) {
  Transform A = GetTestMatrix1();
  Transform B = GetTestMatrix2();

  A.PreConcat(B);
  EXPECT_ROW0_EQ(2036.0f, 2292.0f, 2548.0f, 2804.0f, A);
  EXPECT_ROW1_EQ(2162.0f, 2434.0f, 2706.0f, 2978.0f, A);
  EXPECT_ROW2_EQ(2288.0f, 2576.0f, 2864.0f, 3152.0f, A);
  EXPECT_ROW3_EQ(2414.0f, 2718.0f, 3022.0f, 3326.0f, A);
}

TEST(XFormTest, verifyMakeIdentiy) {
  Transform A = GetTestMatrix1();
  A.MakeIdentity();
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
  EXPECT_TRUE(A.IsIdentity());
}

TEST(XFormTest, verifyTranslate) {
  Transform A;
  A.Translate(2.0, 3.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 3.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that Translate() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale(5.0, 5.0);
  A.Translate(2.0, 3.0);
  EXPECT_ROW0_EQ(5.0f, 0.0f, 0.0f, 10.0f, A);
  EXPECT_ROW1_EQ(0.0f, 5.0f, 0.0f, 15.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  Transform B;
  B.Scale(5.0, 5.0);
  B.Translate(Vector2dF(2.0f, 3.0f));
  EXPECT_EQ(A, B);
}

TEST(XFormTest, verifyPostTranslate) {
  Transform A;
  A.PostTranslate(2.0, 3.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 3.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that PostTranslate() pre-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale(5.0, 5.0);
  A.PostTranslate(2.0, 3.0);
  EXPECT_ROW0_EQ(5.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW1_EQ(0.0f, 5.0f, 0.0f, 3.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  Transform B;
  B.Scale(5.0, 5.0);
  B.PostTranslate(Vector2dF(2.0f, 3.0f));
  EXPECT_EQ(A, B);
}

TEST(XFormTest, verifyTranslate3d) {
  Transform A;
  A.Translate3d(2.0, 3.0, 4.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 3.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 4.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that Translate3d() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.Translate3d(2.0, 3.0, 4.0);
  EXPECT_ROW0_EQ(6.0f, 0.0f, 0.0f, 12.0f, A);
  EXPECT_ROW1_EQ(0.0f, 7.0f, 0.0f, 21.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 8.0f, 32.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  Transform B;
  B.Scale3d(6.0, 7.0, 8.0);
  B.Translate3d(Vector3dF(2.0f, 3.0f, 4.0f));
  EXPECT_EQ(A, B);
}

TEST(XFormTest, verifyPostTranslate3d) {
  Transform A;
  A.PostTranslate3d(2.0, 3.0, 4.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 3.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 4.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that PostTranslate3d() pre-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.PostTranslate3d(2.0, 3.0, 4.0);
  EXPECT_ROW0_EQ(6.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW1_EQ(0.0f, 7.0f, 0.0f, 3.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 8.0f, 4.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  Transform B;
  B.Scale3d(6.0, 7.0, 8.0);
  B.PostTranslate3d(Vector3dF(2.0f, 3.0f, 4.0f));
  EXPECT_EQ(A, B);
}

TEST(XFormTest, verifyScale) {
  Transform A;
  A.Scale(6.0, 7.0);
  EXPECT_ROW0_EQ(6.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(0.0f, 7.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that Scale() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Translate3d(2.0, 3.0, 4.0);
  A.Scale(6.0, 7.0);
  EXPECT_ROW0_EQ(6.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW1_EQ(0.0f, 7.0f, 0.0f, 3.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 4.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyScale3d) {
  Transform A;
  A.Scale3d(6.0, 7.0, 8.0);
  EXPECT_ROW0_EQ(6.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(0.0f, 7.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that scale3d() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Translate3d(2.0, 3.0, 4.0);
  A.Scale3d(6.0, 7.0, 8.0);
  EXPECT_ROW0_EQ(6.0f, 0.0f, 0.0f, 2.0f, A);
  EXPECT_ROW1_EQ(0.0f, 7.0f, 0.0f, 3.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 8.0f, 4.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyPostScale3d) {
  Transform A;
  A.PostScale3d(6.0, 7.0, 8.0);
  EXPECT_ROW0_EQ(6.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(0.0f, 7.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that PostScale3d() pre-multiplies the existing matrix.
  A.MakeIdentity();
  A.Translate3d(2.0, 3.0, 4.0);
  A.PostScale3d(6.0, 7.0, 8.0);
  EXPECT_ROW0_EQ(6.0f, 0.0f, 0.0f, 12.0f, A);
  EXPECT_ROW1_EQ(0.0f, 7.0f, 0.0f, 21.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 8.0f, 32.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, Rotate) {
  Transform A;
  A.Rotate(90.0);
  EXPECT_ROW0_EQ(0.0, -1.0, 0.0, 0.0, A);
  EXPECT_ROW1_EQ(1.0, 0.0, 0.0, 0.0, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that Rotate() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.Rotate(90.0);
  EXPECT_ROW0_EQ(0.0, -6.0, 0.0, 0.0, A);
  EXPECT_ROW1_EQ(7.0, 0.0, 0.0, 0.0, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, RotateAboutXAxis) {
  Transform A;
  double sin45 = 0.5 * sqrt(2.0);
  double cos45 = sin45;

  A.MakeIdentity();
  A.RotateAboutXAxis(90.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(0.0, 0.0, -1.0, 0.0, A);
  EXPECT_ROW2_EQ(0.0, 1.0, 0.0, 0.0, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  A.MakeIdentity();
  A.RotateAboutXAxis(45.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_NEAR(0.0, cos45, -sin45, 0.0, A, kErrorThreshold);
  EXPECT_ROW2_NEAR(0.0, sin45, cos45, 0.0, A, kErrorThreshold);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that RotateAboutXAxis(angle) post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.RotateAboutXAxis(90.0);
  EXPECT_ROW0_EQ(6.0, 0.0, 0.0, 0.0, A);
  EXPECT_ROW1_EQ(0.0, 0.0, -7.0, 0.0, A);
  EXPECT_ROW2_EQ(0.0, 8.0, 0.0, 0.0, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, RotateAboutYAxis) {
  Transform A;
  double sin45 = 0.5 * sqrt(2.0);
  double cos45 = sin45;

  // Note carefully, the expected pattern is inverted compared to rotating
  // about x axis or z axis.
  A.MakeIdentity();
  A.RotateAboutYAxis(90.0);
  EXPECT_ROW0_EQ(0.0, 0.0, 1.0, 0.0, A);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(-1.0, 0.0, 0.0, 0.0, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  A.MakeIdentity();
  A.RotateAboutYAxis(45.0);
  EXPECT_ROW0_NEAR(cos45, 0.0, sin45, 0.0, A, kErrorThreshold);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_NEAR(-sin45, 0.0, cos45, 0.0, A, kErrorThreshold);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that RotateAboutYAxis(angle) post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.RotateAboutYAxis(90.0);
  EXPECT_ROW0_EQ(0.0, 0.0, 6.0, 0.0, A);
  EXPECT_ROW1_EQ(0.0, 7.0, 0.0, 0.0, A);
  EXPECT_ROW2_EQ(-8.0, 0.0, 0.0, 0.0, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, RotateAboutZAxis) {
  Transform A;
  double sin45 = 0.5 * sqrt(2.0);
  double cos45 = sin45;

  A.MakeIdentity();
  A.RotateAboutZAxis(90.0);
  EXPECT_ROW0_EQ(0.0, -1.0, 0.0, 0.0, A);
  EXPECT_ROW1_EQ(1.0, 0.0, 0.0, 0.0, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  A.MakeIdentity();
  A.RotateAboutZAxis(45.0);
  EXPECT_ROW0_NEAR(cos45, -sin45, 0.0, 0.0, A, kErrorThreshold);
  EXPECT_ROW1_NEAR(sin45, cos45, 0.0, 0.0, A, kErrorThreshold);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that RotateAboutZAxis(angle) post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.RotateAboutZAxis(90.0);
  EXPECT_ROW0_EQ(0.0, -6.0, 0.0, 0.0, A);
  EXPECT_ROW1_EQ(7.0, 0.0, 0.0, 0.0, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, RotateAboutForAlignedAxes) {
  Transform A;

  // Check rotation about z-axis
  A.MakeIdentity();
  A.RotateAbout(Vector3dF(0.0, 0.0, 1.0), 90.0);
  EXPECT_ROW0_EQ(0.0, -1.0, 0.0, 0.0, A);
  EXPECT_ROW1_EQ(1.0, 0.0, 0.0, 0.0, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Check rotation about x-axis
  A.MakeIdentity();
  A.RotateAbout(Vector3dF(1.0, 0.0, 0.0), 90.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(0.0, 0.0, -1.0, 0.0, A);
  EXPECT_ROW2_EQ(0.0, 1.0, 0.0, 0.0, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Check rotation about y-axis. Note carefully, the expected pattern is
  // inverted compared to rotating about x axis or z axis.
  A.MakeIdentity();
  A.RotateAbout(Vector3dF(0.0, 1.0, 0.0), 90.0);
  EXPECT_ROW0_EQ(0.0, 0.0, 1.0, 0.0, A);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(-1.0, 0.0, 0.0, 0.0, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that rotate3d(axis, angle) post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.RotateAboutZAxis(90.0);
  EXPECT_ROW0_EQ(0.0, -6.0, 0.0, 0.0, A);
  EXPECT_ROW1_EQ(7.0, 0.0, 0.0, 0.0, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyRotateAboutForArbitraryAxis) {
  // Check rotation about an arbitrary non-axis-aligned vector.
  Transform A;
  A.RotateAbout(Vector3dF(1.0, 1.0, 1.0), 90.0);
  EXPECT_ROW0_NEAR(0.3333333333333334258519187, -0.2440169358562924717404030,
                   0.9106836025229592124219380, 0.0, A, kErrorThreshold);
  EXPECT_ROW1_NEAR(0.9106836025229592124219380, 0.3333333333333334258519187,
                   -0.2440169358562924717404030, 0.0, A, kErrorThreshold);
  EXPECT_ROW2_NEAR(-0.2440169358562924717404030, 0.9106836025229592124219380,
                   0.3333333333333334258519187, 0.0, A, kErrorThreshold);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyRotateAboutForDegenerateAxis) {
  // Check rotation about a degenerate zero vector.
  // It is expected to skip applying the rotation.
  Transform A;

  A.RotateAbout(Vector3dF(0.0, 0.0, 0.0), 45.0);
  // Verify that A remains unchanged.
  EXPECT_TRUE(A.IsIdentity());

  A = GetTestMatrix1();
  A.RotateAbout(Vector3dF(0.0, 0.0, 0.0), 35.0);

  // Verify that A remains unchanged.
  EXPECT_ROW0_EQ(10.0f, 14.0f, 18.0f, 22.0f, A);
  EXPECT_ROW1_EQ(11.0f, 15.0f, 19.0f, 23.0f, A);
  EXPECT_ROW2_EQ(12.0f, 16.0f, 20.0f, 24.0f, A);
  EXPECT_ROW3_EQ(13.0f, 17.0f, 21.0f, 25.0f, A);
}

TEST(XFormTest, verifySkew) {
  // Test a skew along X axis only
  Transform A;
  A.Skew(45.0, 0.0);
  EXPECT_ROW0_EQ(1.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Test a skew along Y axis only
  A.MakeIdentity();
  A.Skew(0.0, 45.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(1.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Verify that skew() post-multiplies the existing matrix. Row 1, column 2,
  // would incorrectly have value "7" if the matrix is pre-multiplied instead
  // of post-multiplied.
  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  A.Skew(45.0, 0.0);
  EXPECT_ROW0_EQ(6.0f, 6.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(0.0f, 7.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 8.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);

  // Test a skew along X and Y axes both
  A.MakeIdentity();
  A.Skew(45.0, 45.0);
  EXPECT_ROW0_EQ(1.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(1.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, 0.0f, 1.0f, A);
}

TEST(XFormTest, verifyPerspectiveDepth) {
  Transform A;
  A.ApplyPerspectiveDepth(1.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, 0.0f, 0.0f, A);
  EXPECT_ROW1_EQ(0.0f, 1.0f, 0.0f, 0.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, -1.0f, 1.0f, A);

  // Verify that PerspectiveDepth() post-multiplies the existing matrix.
  A.MakeIdentity();
  A.Translate3d(2.0, 3.0, 4.0);
  A.ApplyPerspectiveDepth(1.0);
  EXPECT_ROW0_EQ(1.0f, 0.0f, -2.0f, 2.0f, A);
  EXPECT_ROW1_EQ(0.0f, 1.0f, -3.0f, 3.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, -3.0f, 4.0f, A);
  EXPECT_ROW3_EQ(0.0f, 0.0f, -1.0f, 1.0f, A);
}

TEST(XFormTest, verifyHasPerspective) {
  Transform A;
  A.ApplyPerspectiveDepth(1.0);
  EXPECT_TRUE(A.HasPerspective());

  A.MakeIdentity();
  A.ApplyPerspectiveDepth(0.0);
  EXPECT_FALSE(A.HasPerspective());

  A.MakeIdentity();
  A.set_rc(3, 0, -1.f);
  EXPECT_TRUE(A.HasPerspective());

  A.MakeIdentity();
  A.set_rc(3, 1, -1.f);
  EXPECT_TRUE(A.HasPerspective());

  A.MakeIdentity();
  A.set_rc(3, 2, -0.3f);
  EXPECT_TRUE(A.HasPerspective());

  A.MakeIdentity();
  A.set_rc(3, 3, 0.5f);
  EXPECT_TRUE(A.HasPerspective());

  A.MakeIdentity();
  A.set_rc(3, 3, 0.f);
  EXPECT_TRUE(A.HasPerspective());
}

TEST(XFormTest, verifyIsInvertible) {
  Transform A;

  // Translations, rotations, scales, skews and arbitrary combinations of them
  // are invertible.
  A.MakeIdentity();
  EXPECT_TRUE(A.IsInvertible());

  A.MakeIdentity();
  A.Translate3d(2.0, 3.0, 4.0);
  EXPECT_TRUE(A.IsInvertible());

  A.MakeIdentity();
  A.Scale3d(6.0, 7.0, 8.0);
  EXPECT_TRUE(A.IsInvertible());

  A.MakeIdentity();
  A.RotateAboutXAxis(10.0);
  A.RotateAboutYAxis(20.0);
  A.RotateAboutZAxis(30.0);
  EXPECT_TRUE(A.IsInvertible());

  A.MakeIdentity();
  A.Skew(45.0, 0.0);
  EXPECT_TRUE(A.IsInvertible());

  // A perspective matrix (projection plane at z=0) is invertible. The
  // intuitive explanation is that perspective is equivalent to a skew of the
  // w-axis; skews are invertible.
  A.MakeIdentity();
  A.ApplyPerspectiveDepth(1.0);
  EXPECT_TRUE(A.IsInvertible());

  // A "pure" perspective matrix derived by similar triangles, with rc(3, 3) set
  // to zero (i.e. camera positioned at the origin), is not invertible.
  A.MakeIdentity();
  A.ApplyPerspectiveDepth(1.0);
  A.set_rc(3, 3, 0.f);
  EXPECT_FALSE(A.IsInvertible());

  // Adding more to a non-invertible matrix will not make it invertible in the
  // general case.
  A.MakeIdentity();
  A.ApplyPerspectiveDepth(1.0);
  A.set_rc(3, 3, 0.f);
  EXPECT_FALSE(A.IsInvertible());
  A.Scale3d(6.0, 7.0, 8.0);
  EXPECT_FALSE(A.IsInvertible());
  A.RotateAboutXAxis(10.0);
  EXPECT_FALSE(A.IsInvertible());
  A.RotateAboutYAxis(20.0);
  EXPECT_FALSE(A.IsInvertible());
  A.RotateAboutZAxis(30.0);
  EXPECT_FALSE(A.IsInvertible());
  A.Translate3d(6.0, 7.0, 8.0);
  if (A.IsInvertible()) {
    // Due to some computation errors, now A may become invertible with a tiny
    // determinant.
    EXPECT_NEAR(A.Determinant(), 0.0, 1e-12);
  }

  // A degenerate matrix of all zeros is not invertible.
  A.MakeIdentity();
  A.set_rc(0, 0, 0.f);
  A.set_rc(1, 1, 0.f);
  A.set_rc(2, 2, 0.f);
  A.set_rc(3, 3, 0.f);
  EXPECT_FALSE(A.IsInvertible());
}

TEST(XFormTest, verifyIsIdentity) {
  Transform A = GetTestMatrix1();
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  EXPECT_TRUE(A.IsIdentity());

  // Modifying any one individual element should cause the matrix to no longer
  // be identity.
  A.MakeIdentity();
  A.set_rc(0, 0, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(1, 0, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(2, 0, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(3, 0, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(0, 1, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(1, 1, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(2, 1, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(3, 1, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(0, 2, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(1, 2, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(2, 2, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(3, 2, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(0, 3, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(1, 3, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(2, 3, 2.f);
  EXPECT_FALSE(A.IsIdentity());

  A.MakeIdentity();
  A.set_rc(3, 3, 2.f);
  EXPECT_FALSE(A.IsIdentity());
}

TEST(XFormTest, verifyIsIdentityOrTranslation) {
  Transform A = GetTestMatrix1();
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  EXPECT_TRUE(A.IsIdentityOrTranslation());

  // Modifying any non-translation components should cause
  // IsIdentityOrTranslation() to return false. NOTE: (0, 3), (1, 3), and
  // (2, 3) are the translation components, so modifying them should still
  // return true.
  A.MakeIdentity();
  A.set_rc(0, 0, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(1, 0, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(2, 0, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 0, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(0, 1, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(1, 1, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(2, 1, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 1, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(0, 2, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(1, 2, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(2, 2, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 2, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(0, 3, 2.f);
  EXPECT_TRUE(A.IsIdentityOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(1, 3, 2.f);
  EXPECT_TRUE(A.IsIdentityOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(2, 3, 2.f);
  EXPECT_TRUE(A.IsIdentityOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 3, 2.f);
  EXPECT_FALSE(A.IsIdentityOrTranslation());
}

TEST(XFormTest, ApproximatelyIdentityOrTranslation) {
  constexpr double kBigError = 1e-4;
  constexpr double kSmallError = std::numeric_limits<float>::epsilon() / 2.0;

  // Exact pure translation.
  Transform a;
  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrIntegerTranslation(kBigError));

  // Set translate values to integer values other than 0 or 1.
  a.set_rc(0, 3, 3);
  a.set_rc(1, 3, 4);
  a.set_rc(2, 3, 5);

  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrIntegerTranslation(kBigError));

  // Set translate values to values other than 0 or 1.
  a.set_rc(0, 3, 3.4f);
  a.set_rc(1, 3, 4.4f);
  a.set_rc(2, 3, 5.6f);

  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(kBigError));

  // Approximately pure translation.
  a = ApproxIdentityMatrix(kBigError);

  // All these are false because the perspective error is bigger than the
  // allowed std::min(float_epsilon, tolerance);
  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(kSmallError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(kSmallError));

  // Set perspective components to be exact identity.
  a.set_rc(3, 0, 0);
  a.set_rc(3, 1, 0);
  a.set_rc(3, 2, 0);
  a.set_rc(3, 3, 1);

  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(kSmallError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrIntegerTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(kSmallError));

  // Set translate values to values other than 0 or 1.
  // The error is set to kBigError / 2 instead of kBigError because the
  // arithmetic may make the error bigger.
  a.set_rc(0, 3, 3.0 + kBigError / 2);
  a.set_rc(1, 3, 4.0 + kBigError / 2);
  a.set_rc(2, 3, 5.0 + kBigError / 2);

  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(kSmallError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrIntegerTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(kSmallError));

  // Set translate values to values other than 0 or 1.
  a.set_rc(0, 3, 3.4f);
  a.set_rc(1, 3, 4.4f);
  a.set_rc(2, 3, 5.6f);

  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(kSmallError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(kSmallError));

  // Test with kSmallError in the matrix.
  a = ApproxIdentityMatrix(kSmallError);

  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(kSmallError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_TRUE(a.IsApproximatelyIdentityOrIntegerTranslation(kSmallError));

  // Set some values (not translate values) to values other than 0 or 1.
  a.set_rc(0, 1, 3.4f);
  a.set_rc(3, 2, 4.4f);
  a.set_rc(2, 0, 5.6f);

  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(0));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrTranslation(kSmallError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(0));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(kBigError));
  EXPECT_FALSE(a.IsApproximatelyIdentityOrIntegerTranslation(kSmallError));
}

TEST(XFormTest, RoundToIdentityOrIntegerTranslation) {
  Transform a = ApproxIdentityMatrix(0.1);
  EXPECT_FALSE(a.IsIdentityOrIntegerTranslation());
  a.RoundToIdentityOrIntegerTranslation();
  EXPECT_TRUE(a.IsIdentity());
  EXPECT_TRUE(a.IsIdentityOrIntegerTranslation());

  a.Translate3d(1.1, 2.2, 3.8);
  EXPECT_FALSE(a.IsIdentityOrIntegerTranslation());
  a.RoundToIdentityOrIntegerTranslation();
  EXPECT_TRUE(a.IsIdentityOrIntegerTranslation());
  EXPECT_EQ(1.0, a.rc(0, 3));
  EXPECT_EQ(2.0, a.rc(1, 3));
  EXPECT_EQ(4.0, a.rc(2, 3));
}

TEST(XFormTest, verifyIsScaleOrTranslation) {
  EXPECT_TRUE(Transform().IsScaleOrTranslation());
  EXPECT_TRUE(Transform::MakeScale(2, 3).IsScaleOrTranslation());
  EXPECT_TRUE(Transform::MakeTranslation(4, 5).IsScaleOrTranslation());
  EXPECT_TRUE((Transform::MakeTranslation(4, 5) * Transform::MakeScale(2, 3))
                  .IsScaleOrTranslation());

  Transform A = GetTestMatrix1();
  EXPECT_FALSE(A.IsScaleOrTranslation());

  // Modifying any non-scale or non-translation components should cause
  // IsScaleOrTranslation() to return false. (0, 0), (1, 1), (2, 2), (0, 3),
  // (1, 3), and (2, 3) are the scale and translation components, so
  // modifying them should still return true.

  // Note carefully - expecting true here.
  A.MakeIdentity();
  EXPECT_TRUE(A.IsScaleOrTranslation());
  A.set_rc(0, 0, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(1, 0, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(2, 0, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 0, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(0, 1, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(1, 1, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(2, 1, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 1, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(0, 2, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(1, 2, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(2, 2, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 2, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(0, 3, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(1, 3, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  // Note carefully - expecting true here.
  A.MakeIdentity();
  A.set_rc(2, 3, 2.f);
  EXPECT_TRUE(A.IsScaleOrTranslation());

  A.MakeIdentity();
  A.set_rc(3, 3, 2.f);
  EXPECT_FALSE(A.IsScaleOrTranslation());
}

TEST(XFormTest, To2dScale) {
  Transform t;
  EXPECT_TRUE(t.IsScale2d());
  EXPECT_EQ(Vector2dF(1, 1), t.To2dScale());

  t.Scale(2.5f, 3.75f);
  EXPECT_TRUE(t.IsScale2d());
  EXPECT_EQ(Vector2dF(2.5f, 3.75f), t.To2dScale());

  t.EnsureFullMatrixForTesting();
  EXPECT_TRUE(t.IsScale2d());
  EXPECT_EQ(Vector2dF(2.5f, 3.75f), t.To2dScale());

  t.Scale3d(3, 4, 5);
  EXPECT_FALSE(t.IsScale2d());
  EXPECT_EQ(Vector2dF(7.5f, 15.f), t.To2dScale());

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      t.MakeIdentity();
      t.set_rc(row, col, 100);
      bool is_scale_2d = row == col && (row == 0 || row == 1);
      EXPECT_EQ(is_scale_2d, t.IsScale2d()) << " row=" << row << " col=" << col;
    }
  }
}

TEST(XFormTest, Flatten) {
  Transform A = GetTestMatrix1();
  EXPECT_FALSE(A.IsFlat());

  A.Flatten();
  EXPECT_ROW0_EQ(10.0f, 14.0f, 0.0f, 22.0f, A);
  EXPECT_ROW1_EQ(11.0f, 15.0f, 0.0f, 23.0f, A);
  EXPECT_ROW2_EQ(0.0f, 0.0f, 1.0f, 0.0f, A);
  EXPECT_ROW3_EQ(13.0f, 17.0f, 0.0f, 25.0f, A);

  EXPECT_TRUE(A.IsFlat());
}

TEST(XFormTest, IsFlat) {
  Transform transform = GetTestMatrix1();

  // A transform with all entries non-zero isn't flat.
  EXPECT_FALSE(transform.IsFlat());

  transform.set_rc(0, 2, 0.f);
  transform.set_rc(1, 2, 0.f);
  transform.set_rc(2, 2, 1.f);
  transform.set_rc(3, 2, 0.f);

  EXPECT_FALSE(transform.IsFlat());

  transform.set_rc(2, 0, 0.f);
  transform.set_rc(2, 1, 0.f);
  transform.set_rc(2, 3, 0.f);

  // Since the third column and row are both (0, 0, 1, 0), the transform is
  // flat.
  EXPECT_TRUE(transform.IsFlat());
}

TEST(XFormTest, Preserves2dAffine) {
  static const struct TestCase {
    gfx::Transform transform;
    bool expected;
  } test_cases[] = {
      // Skew z axis in x and y direction
      {
          gfx::Transform::ColMajor(1.0, 0.0, 0.0, 0.0,  //
                                   0.0, 1.0, 0.0, 0.0,  //
                                   0.1, 0.1, 1.0, 0.0,  //
                                   0.0, 0.0, 0.0, 1.0),
          true,
      },

      // Scale z axis
      {
          gfx::Transform::ColMajor(1.0, 0.0, 0.0, 0.0,  //
                                   0.0, 1.0, 0.0, 0.0,  //
                                   0.0, 0.0, 2.0, 0.0,  //
                                   0.0, 0.0, 0.0, 1.0),
          true,
      },

      // Perspective projection along the z axis
      {
          gfx::Transform::ColMajor(1.0, 0.0, 0.0, 0.0,  //
                                   0.0, 1.0, 0.0, 0.0,  //
                                   0.0, 0.0, 1.0, 0.1,  //
                                   0.0, 0.0, 0.0, 1.0),
          true,
      },

      // All together, including x and y axis skew and translation
      {
          gfx::Transform::ColMajor(1.0, 0.1, 0.0, 0.0,  //
                                   0.1, 1.0, 0.0, 0.0,  //
                                   0.1, 0.1, 2.0, 0.1,  //
                                   0.1, 0.1, 0.0, 1.0),
          true,
      },

      // Skew x axis in the z direction.
      {
          gfx::Transform::ColMajor(1.0, 0.0, 0.1, 0.0,  //
                                   0.0, 1.0, 0.0, 0.0,  //
                                   0.0, 0.0, 1.0, 0.0,  //
                                   0.0, 0.0, 0.0, 1.0),
          false,
      },

      // Add y perspective
      {
          gfx::Transform::ColMajor(1.0, 0.0, 0.0, 0.0,  //
                                   0.0, 1.0, 0.0, 0.1,  //
                                   0.0, 0.0, 1.0, 0.0,  //
                                   0.0, 0.0, 0.0, 1.0),
          false,
      },

      // Add z translation
      {
          gfx::Transform::ColMajor(1.0, 0.0, 0.0, 0.0,  //
                                   0.0, 1.0, 0.0, 0.1,  //
                                   0.0, 0.0, 1.0, 0.0,  //
                                   0.0, 0.0, 0.1, 1.0),
          false,
      },
  };

  // Another implementation of Preserves2dAffine that isn't as fast, good for
  // testing the faster implementation.
  auto EmpiricallyPreserves2dAffine = [](const Transform& transform) {
    Point3F p1(5.0f, 5.0f, 0.0f);
    Point3F p2(10.0f, 5.0f, 0.0f);
    Point3F p3(10.0f, 20.0f, 0.0f);
    Point3F p4(5.0f, 20.0f, 0.0f);

    QuadF test_quad(PointF(p1.x(), p1.y()), PointF(p2.x(), p2.y()),
                    PointF(p3.x(), p3.y()), PointF(p4.x(), p4.y()));
    EXPECT_TRUE(test_quad.IsRectilinear());

    p1 = transform.MapPoint(p1);
    p2 = transform.MapPoint(p2);
    p3 = transform.MapPoint(p3);
    p4 = transform.MapPoint(p4);

    // We expect our quad on the x/y plane to remain so.
    if (p1.z() != 0 || p2.z() != 0 || p3.z() != 0 || p4.z() != 0) {
      return false;
    }

    // In an affine transform, parallel lines are preserved.
    return CrossProduct(p2 - p1, p3 - p4).IsZero() &&
           CrossProduct(p4 - p1, p3 - p2).IsZero();
  };

  for (const auto& value : test_cases) {
    SCOPED_TRACE(base::StringPrintf("transform = %s, expected = %d",
                                    value.transform.ToString().c_str(),
                                    value.expected));

    if (value.expected) {
      EXPECT_TRUE(EmpiricallyPreserves2dAffine(value.transform));
      EXPECT_TRUE(value.transform.Preserves2dAffine());
    } else {
      EXPECT_FALSE(EmpiricallyPreserves2dAffine(value.transform));
      EXPECT_FALSE(value.transform.Preserves2dAffine());
    }
  }
}

// Another implementation of Preserves2dAxisAlignment that isn't as fast,
// good for testing the faster implementation.
static bool EmpiricallyPreserves2dAxisAlignment(const Transform& transform) {
  Point3F p1(5.0f, 5.0f, 0.0f);
  Point3F p2(10.0f, 5.0f, 0.0f);
  Point3F p3(10.0f, 20.0f, 0.0f);
  Point3F p4(5.0f, 20.0f, 0.0f);

  QuadF test_quad(PointF(p1.x(), p1.y()), PointF(p2.x(), p2.y()),
                  PointF(p3.x(), p3.y()), PointF(p4.x(), p4.y()));
  EXPECT_TRUE(test_quad.IsRectilinear());

  p1 = transform.MapPoint(p1);
  p2 = transform.MapPoint(p2);
  p3 = transform.MapPoint(p3);
  p4 = transform.MapPoint(p4);

  QuadF transformedQuad(PointF(p1.x(), p1.y()), PointF(p2.x(), p2.y()),
                        PointF(p3.x(), p3.y()), PointF(p4.x(), p4.y()));
  return transformedQuad.IsRectilinear();
}

TEST(XFormTest, Preserves2dAxisAlignment) {
  static const struct TestCase {
    double a;  // row 1, column 1
    double b;  // row 1, column 2
    double c;  // row 2, column 1
    double d;  // row 2, column 2
    bool expected;
    bool degenerate;
  } test_cases[] = {
      // clang-format off
      { 3.0, 0.0,
        0.0, 4.0, true, false },  // basic case
      { 0.0, 4.0,
        3.0, 0.0, true, false },  // rotate by 90
      { 0.0, 0.0,
        0.0, 4.0, true, true },   // degenerate x
      { 3.0, 0.0,
        0.0, 0.0, true, true },   // degenerate y
      { 0.0, 0.0,
        3.0, 0.0, true, true },   // degenerate x + rotate by 90
      { 0.0, 4.0,
        0.0, 0.0, true, true },   // degenerate y + rotate by 90
      { 3.0, 4.0,
        0.0, 0.0, false, true },
      { 0.0, 0.0,
        3.0, 4.0, false, true },
      { 0.0, 3.0,
        0.0, 4.0, false, true },
      { 3.0, 0.0,
        4.0, 0.0, false, true },
      { 3.0, 4.0,
        5.0, 0.0, false, false },
      { 3.0, 4.0,
        0.0, 5.0, false, false },
      { 3.0, 0.0,
        4.0, 5.0, false, false },
      { 0.0, 3.0,
        4.0, 5.0, false, false },
      { 2.0, 3.0,
        4.0, 5.0, false, false },
      // clang-format on
  };

  Transform transform;
  for (const auto& value : test_cases) {
    transform.MakeIdentity();
    transform.set_rc(0, 0, value.a);
    transform.set_rc(0, 1, value.b);
    transform.set_rc(1, 0, value.c);
    transform.set_rc(1, 1, value.d);

    if (value.expected) {
      EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
      EXPECT_TRUE(transform.Preserves2dAxisAlignment());
      if (value.degenerate) {
        EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
      } else {
        EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());
      }
    } else {
      EXPECT_FALSE(EmpiricallyPreserves2dAxisAlignment(transform));
      EXPECT_FALSE(transform.Preserves2dAxisAlignment());
      EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
    }
  }

  // Try the same test cases again, but this time make sure that other matrix
  // elements (except perspective) have entries, to test that they are ignored.
  for (const auto& value : test_cases) {
    transform.MakeIdentity();
    transform.set_rc(0, 0, value.a);
    transform.set_rc(0, 1, value.b);
    transform.set_rc(1, 0, value.c);
    transform.set_rc(1, 1, value.d);

    transform.set_rc(0, 2, 1.f);
    transform.set_rc(0, 3, 2.f);
    transform.set_rc(1, 2, 3.f);
    transform.set_rc(1, 3, 4.f);
    transform.set_rc(2, 0, 5.f);
    transform.set_rc(2, 1, 6.f);
    transform.set_rc(2, 2, 7.f);
    transform.set_rc(2, 3, 8.f);

    if (value.expected) {
      EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
      EXPECT_TRUE(transform.Preserves2dAxisAlignment());
      if (value.degenerate) {
        EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
      } else {
        EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());
      }
    } else {
      EXPECT_FALSE(EmpiricallyPreserves2dAxisAlignment(transform));
      EXPECT_FALSE(transform.Preserves2dAxisAlignment());
      EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
    }
  }

  // Try the same test cases again, but this time add perspective which is
  // always assumed to not-preserve axis alignment.
  for (const auto& value : test_cases) {
    transform.MakeIdentity();
    transform.set_rc(0, 0, value.a);
    transform.set_rc(0, 1, value.b);
    transform.set_rc(1, 0, value.c);
    transform.set_rc(1, 1, value.d);

    transform.set_rc(0, 2, 1.f);
    transform.set_rc(0, 3, 2.f);
    transform.set_rc(1, 2, 3.f);
    transform.set_rc(1, 3, 4.f);
    transform.set_rc(2, 0, 5.f);
    transform.set_rc(2, 1, 6.f);
    transform.set_rc(2, 2, 7.f);
    transform.set_rc(2, 3, 8.f);
    transform.set_rc(3, 0, 9.f);
    transform.set_rc(3, 1, 10.f);
    transform.set_rc(3, 2, 11.f);
    transform.set_rc(3, 3, 12.f);

    EXPECT_FALSE(EmpiricallyPreserves2dAxisAlignment(transform));
    EXPECT_FALSE(transform.Preserves2dAxisAlignment());
    EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
  }

  // Try a few more practical situations to check precision
  transform.MakeIdentity();
  constexpr double kNear90Degrees = 90.0 + kErrorThreshold / 2;
  transform.RotateAboutZAxis(kNear90Degrees);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutZAxis(kNear90Degrees * 2);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutZAxis(kNear90Degrees * 3);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutYAxis(kNear90Degrees);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutXAxis(kNear90Degrees);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutZAxis(kNear90Degrees);
  transform.RotateAboutYAxis(kNear90Degrees);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutZAxis(kNear90Degrees);
  transform.RotateAboutXAxis(kNear90Degrees);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutYAxis(kNear90Degrees);
  transform.RotateAboutZAxis(kNear90Degrees);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutZAxis(45.0);
  EXPECT_FALSE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_FALSE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  // 3-d case; In 2d after an orthographic projection, this case does
  // preserve 2d axis alignment. But in 3d, it does not preserve axis
  // alignment.
  transform.MakeIdentity();
  transform.RotateAboutYAxis(45.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.RotateAboutXAxis(45.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  // Perspective cases.
  transform.MakeIdentity();
  transform.ApplyPerspectiveDepth(10.0);
  transform.RotateAboutYAxis(45.0);
  EXPECT_FALSE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_FALSE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.ApplyPerspectiveDepth(10.0);
  transform.RotateAboutZAxis(90.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  transform.MakeIdentity();
  transform.ApplyPerspectiveDepth(-10.0);
  transform.RotateAboutZAxis(90.0);
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_TRUE(transform.NonDegeneratePreserves2dAxisAlignment());

  // To be non-degenerate, the constant contribution to perspective must
  // be positive.

  // clang-format off
  transform = Transform::RowMajor(1.0, 0.0, 0.0, 0.0,
                                  0.0, 1.0, 0.0, 0.0,
                                  0.0, 0.0, 1.0, 0.0,
                                  0.0, 0.0, 0.0, -1.0);
  // clang-format on
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());

  // clang-format off
  transform = Transform::RowMajor(2.0, 0.0, 0.0, 0.0,
                                  0.0, 5.0, 0.0, 0.0,
                                  0.0, 0.0, 1.0, 0.0,
                                  0.0, 0.0, 0.0, 0.0);
  // clang-format on
  EXPECT_TRUE(EmpiricallyPreserves2dAxisAlignment(transform));
  EXPECT_TRUE(transform.Preserves2dAxisAlignment());
  EXPECT_FALSE(transform.NonDegeneratePreserves2dAxisAlignment());
}

TEST(XFormTest, To2dTranslation) {
  Vector2dF translation(3.f, 7.f);
  Transform transform;
  transform.Translate(translation.x(), translation.y() + 1);
  EXPECT_NE(translation, transform.To2dTranslation());
  transform.MakeIdentity();
  transform.Translate(translation.x(), translation.y());
  transform.set_rc(1, 1, 100);
  EXPECT_EQ(translation, transform.To2dTranslation());
}

TEST(XFormTest, To3dTranslation) {
  Transform transform;
  EXPECT_EQ(gfx::Vector3dF(), transform.To3dTranslation());
  transform.Translate(10, 20);
  EXPECT_EQ(gfx::Vector3dF(10, 20, 0), transform.To3dTranslation());
  transform.Translate3d(20, -60, -10);
  EXPECT_EQ(gfx::Vector3dF(30, -40, -10), transform.To3dTranslation());
  transform.set_rc(1, 1, 100);
  EXPECT_EQ(gfx::Vector3dF(30, -40, -10), transform.To3dTranslation());
}

TEST(XFormTest, MapRect) {
  Transform translation = Transform::MakeTranslation(3.25f, 7.75f);
  RectF rect(1.25f, 2.5f, 3.75f, 4.f);
  RectF expected(4.5f, 10.25f, 3.75f, 4.f);
  EXPECT_EQ(expected, translation.MapRect(rect));

  EXPECT_EQ(rect, Transform().MapRect(rect));

  auto singular = Transform::MakeScale(0.f);
  EXPECT_EQ(RectF(0, 0, 0, 0), singular.MapRect(rect));

  auto negative_scale = Transform::MakeScale(-1, -2);
  EXPECT_EQ(RectF(-5.f, -13.f, 3.75f, 8.f), negative_scale.MapRect(rect));

  auto rotate = Transform::Make90degRotation();
  EXPECT_EQ(RectF(-6.5f, 1.25f, 4.f, 3.75f), rotate.MapRect(rect));
}

TEST(XFormTest, MapIntRect) {
  auto translation = Transform::MakeTranslation(3.25f, 7.75f);
  EXPECT_EQ(Rect(4, 9, 4, 5), translation.MapRect(Rect(1, 2, 3, 4)));

  EXPECT_EQ(Rect(1, 2, 3, 4), Transform().MapRect(Rect(1, 2, 3, 4)));

  auto singular = Transform::MakeScale(0.f);
  EXPECT_EQ(Rect(0, 0, 0, 0), singular.MapRect(Rect(1, 2, 3, 4)));
}

TEST(XFormTest, TransformRectReverse) {
  auto translation = Transform::MakeTranslation(3.25f, 7.75f);
  RectF rect(1.25f, 2.5f, 3.75f, 4.f);
  RectF expected(-2.f, -5.25f, 3.75f, 4.f);
  EXPECT_EQ(expected, translation.InverseMapRect(rect));

  EXPECT_EQ(rect, Transform().InverseMapRect(rect));

  auto singular = Transform::MakeScale(0.f);
  EXPECT_FALSE(singular.InverseMapRect(rect));

  auto negative_scale = Transform::MakeScale(-1, -2);
  EXPECT_EQ(RectF(-5.f, -3.25f, 3.75f, 2.f),
            negative_scale.InverseMapRect(rect));

  auto rotate = Transform::Make90degRotation();
  EXPECT_EQ(RectF(2.5f, -5.f, 4.f, 3.75f), rotate.InverseMapRect(rect));
}

TEST(XFormTest, InverseMapIntRect) {
  auto translation = Transform::MakeTranslation(3.25f, 7.75f);
  EXPECT_EQ(Rect(-3, -6, 4, 5), translation.InverseMapRect(Rect(1, 2, 3, 4)));

  EXPECT_EQ(Rect(1, 2, 3, 4), Transform().InverseMapRect(Rect(1, 2, 3, 4)));

  auto singular = Transform::MakeScale(0.f);
  EXPECT_FALSE(singular.InverseMapRect(Rect(1, 2, 3, 4)));
}

TEST(XFormTest, MapQuad) {
  auto translation = Transform::MakeTranslation(3.25f, 7.75f);
  QuadF q(PointF(1.25f, 2.5f), PointF(3.75f, 4.f), PointF(23.f, 45.f),
          PointF(12.f, 67.f));
  EXPECT_EQ(QuadF(PointF(4.5f, 10.25f), PointF(7.f, 11.75f),
                  PointF(26.25f, 52.75f), PointF(15.25f, 74.75f)),
            translation.MapQuad(q));

  EXPECT_EQ(q, Transform().MapQuad(q));

  auto singular = Transform::MakeScale(0.f);
  EXPECT_EQ(QuadF(), singular.MapQuad(q));

  auto negative_scale = Transform::MakeScale(-1, -2);
  EXPECT_EQ(QuadF(PointF(-1.25f, -5.f), PointF(-3.75f, -8.f),
                  PointF(-23.f, -90.f), PointF(-12.f, -134.f)),
            negative_scale.MapQuad(q));

  auto rotate = Transform::Make90degRotation();
  EXPECT_EQ(QuadF(PointF(-2.5f, 1.25f), PointF(-4.f, 3.75f),
                  PointF(-45.f, 23.f), PointF(-67.f, 12.f)),
            rotate.MapQuad(q));
}

TEST(XFormTest, MapBox) {
  Transform translation;
  translation.Translate3d(3.f, 7.f, 6.f);
  BoxF box(1.f, 2.f, 3.f, 4.f, 5.f, 6.f);
  BoxF expected(4.f, 9.f, 9.f, 4.f, 5.f, 6.f);
  BoxF transformed = translation.MapBox(box);
  EXPECT_EQ(expected, transformed);
}

TEST(XFormTest, Round2dTranslationComponents) {
  Transform translation;
  Transform expected;

  translation.Round2dTranslationComponents();
  EXPECT_EQ(expected.ToString(), translation.ToString());

  translation.Translate(1.0f, 1.0f);
  expected.Translate(1.0f, 1.0f);
  translation.Round2dTranslationComponents();
  EXPECT_EQ(expected.ToString(), translation.ToString());

  translation.Translate(0.5f, 0.4f);
  expected.Translate(1.0f, 0.0f);
  translation.Round2dTranslationComponents();
  EXPECT_EQ(expected.ToString(), translation.ToString());

  // Rounding should only affect 2d translation components.
  translation.Translate3d(0.f, 0.f, 0.5f);
  expected.Translate3d(0.f, 0.f, 0.5f);
  translation.Round2dTranslationComponents();
  EXPECT_EQ(expected.ToString(), translation.ToString());
}

TEST(XFormTest, BackFaceVisiblilityTolerance) {
  Transform backface_invisible;
  backface_invisible.set_rc(0, 3, 1.f);
  backface_invisible.set_rc(3, 0, 1.f);
  backface_invisible.set_rc(2, 0, 1.f);
  backface_invisible.set_rc(3, 2, 1.f);

  // The transformation matrix has a determinant = 1 and cofactor33 = 0. So,
  // IsBackFaceVisible should return false.
  EXPECT_EQ(backface_invisible.Determinant(), 1.f);
  EXPECT_FALSE(backface_invisible.IsBackFaceVisible());

  // Adding a noise to the transformsation matrix that is within the tolerance
  // (machine epsilon) should not change the result.
  float noise = std::numeric_limits<float>::epsilon();
  backface_invisible.set_rc(0, 3, 1.f + noise);
  EXPECT_FALSE(backface_invisible.IsBackFaceVisible());

  // A noise that is more than the tolerance should change the result.
  backface_invisible.set_rc(0, 3, 1.f + (2 * noise));
  EXPECT_TRUE(backface_invisible.IsBackFaceVisible());
}

TEST(XFormTest, TransformVector4) {
  Transform transform;
  transform.set_rc(0, 0, 2.5f);
  transform.set_rc(1, 1, 3.5f);
  transform.set_rc(2, 2, 4.5f);
  transform.set_rc(3, 3, 5.5f);
  std::array<float, 4> input = {11.5f, 22.5f, 33.5f, 44.5f};
  auto vector = input;
  std::array<float, 4> expected = {28.75f, 78.75f, 150.75f, 244.75f};
  transform.TransformVector4(vector.data());
  EXPECT_EQ(expected, vector);

  // With translations and perspectives.
  transform.set_rc(0, 3, 10);
  transform.set_rc(1, 3, 20);
  transform.set_rc(2, 3, 30);
  transform.set_rc(3, 0, 40);
  transform.set_rc(3, 1, 50);
  transform.set_rc(3, 2, 60);
  vector = input;
  expected = {473.75f, 968.75f, 1485.75f, 3839.75f};
  transform.TransformVector4(vector.data());
  EXPECT_EQ(expected, vector);

  // TransformVector4 with simple 2d transform.
  transform =
      Transform::MakeTranslation(10, 20) * Transform::MakeScale(2.5f, 3.5f);
  vector = input;
  expected = {473.75f, 968.75f, 33.5f, 44.5f};
  transform.TransformVector4(vector.data());
  EXPECT_EQ(expected, vector);

  vector = input;
  transform.EnsureFullMatrixForTesting();
  transform.TransformVector4(vector.data());
  EXPECT_EQ(expected, vector);
}

TEST(XFormTest, Make90NRotation) {
  auto t1 = Transform::Make90degRotation();
  EXPECT_EQ(gfx::PointF(-50, 100), t1.MapPoint(gfx::PointF(100, 50)));

  auto t2 = Transform::Make180degRotation();
  EXPECT_EQ(Transform::MakeScale(-1), t2);
  EXPECT_EQ(gfx::PointF(-100, -50), t2.MapPoint(gfx::PointF(100, 50)));

  auto t3 = Transform::Make270degRotation();
  EXPECT_EQ(gfx::PointF(50, -100), t3.MapPoint(gfx::PointF(100, 50)));

  auto t4 = t1 * t1;
  EXPECT_EQ(t2, t4);
  t4.PreConcat(t1);
  EXPECT_EQ(t3, t4);
  t4.PreConcat(t1);
  EXPECT_TRUE(t4.IsIdentity());
  t2.PreConcat(t2);
  EXPECT_TRUE(t2.IsIdentity());
}

TEST(XFormTest, Rotate90NDegrees) {
  Transform t1;
  t1.Rotate(90);
  EXPECT_EQ(Transform::Make90degRotation(), t1);

  Transform t2;
  t2.Rotate(180);
  EXPECT_EQ(Transform::Make180degRotation(), t2);

  Transform t3;
  t3.Rotate(270);
  EXPECT_EQ(Transform::Make270degRotation(), t3);

  Transform t4;
  t4.Rotate(360);
  EXPECT_EQ(Transform(), t4);
  t4.Rotate(-270);
  EXPECT_EQ(t1, t4);
  t4.Rotate(-180);
  EXPECT_EQ(t3, t4);
  t4.Rotate(270);
  EXPECT_EQ(t2, t4);

  t1.Rotate(-90);
  t2.Rotate(180);
  t3.Rotate(-270);
  t4.Rotate(-180);
  EXPECT_TRUE(t1.IsIdentity());
  EXPECT_TRUE(t2.IsIdentity());
  EXPECT_TRUE(t3.IsIdentity());
  EXPECT_TRUE(t4.IsIdentity());

  // This should not crash. https://crbug.com/1378323.
  Transform t;
  t.Rotate(-1e-30);
}

TEST(XFormTest, MapPoint) {
  Transform transform;
  transform.Translate3d(1.25f, 2.75f, 3.875f);
  transform.Scale3d(3, 4, 5);
  EXPECT_EQ(PointF(38.75f, 140.75f), transform.MapPoint(PointF(12.5f, 34.5f)));
  EXPECT_EQ(Point3F(38.75f, 140.75f, 286.375f),
            transform.MapPoint(Point3F(12.5f, 34.5f, 56.5f)));

  transform.MakeIdentity();
  transform.set_rc(3, 0, 0.5);
  transform.set_rc(3, 1, 2);
  transform.set_rc(3, 2, 0.75);
  EXPECT_POINTF_EQ(PointF(0.2, 0.4), transform.MapPoint(PointF(2, 4)));
  EXPECT_POINT3F_EQ(Point3F(0.18181818f, 0.27272727f, 0.36363636f),
                    transform.MapPoint(Point3F(2, 3, 4)));

  // 0 in all perspectives should be ignored.
  transform.MakeIdentity();
  transform.Translate3d(10, 20, 30);
  transform.set_rc(3, 3, 0);
  EXPECT_EQ(PointF(12, 24), transform.MapPoint(PointF(2, 4)));
  EXPECT_EQ(Point3F(12, 23, 34), transform.MapPoint(Point3F(2, 3, 4)));

  // NaN in perspective should be ignored.
  transform.set_rc(3, 3, std::numeric_limits<float>::quiet_NaN());
  EXPECT_EQ(PointF(12, 24), transform.MapPoint(PointF(2, 4)));
  EXPECT_EQ(Point3F(12, 23, 34), transform.MapPoint(Point3F(2, 3, 4)));

  // MapPoint with simple 2d transform.
  transform = Transform::MakeTranslation(10, 20) * Transform::MakeScale(3, 4);
  EXPECT_EQ(PointF(47.5f, 158.0f), transform.MapPoint(PointF(12.5f, 34.5f)));
  EXPECT_EQ(Point3F(47.5f, 158.0f, 56.5f),
            transform.MapPoint(Point3F(12.5f, 34.5f, 56.5f)));

  transform.EnsureFullMatrixForTesting();
  EXPECT_EQ(PointF(47.5f, 158.0f), transform.MapPoint(PointF(12.5f, 34.5f)));
  EXPECT_EQ(Point3F(47.5f, 158.0f, 56.5f),
            transform.MapPoint(Point3F(12.5f, 34.5f, 56.5f)));
}

TEST(XFormTest, InverseMapPoint) {
  Transform transform;
  transform.Translate(1, 2);
  transform.Rotate(70);
  transform.Scale(3, 4);
  transform.Skew(30, 70);

  const PointF point_f(12.34f, 56.78f);
  PointF transformed_point_f = transform.MapPoint(point_f);
  const std::optional<PointF> reverted_point_f =
      transform.InverseMapPoint(transformed_point_f);
  ASSERT_TRUE(reverted_point_f.has_value());
  EXPECT_TRUE(PointsAreNearlyEqual(reverted_point_f.value(), point_f));

  const Point point(12, 13);
  Point transformed_point = transform.MapPoint(point);
  EXPECT_EQ(point, transform.InverseMapPoint(transformed_point));

  Transform transform3d;
  transform3d.Translate3d(1, 2, 3);
  transform3d.RotateAbout(Vector3dF(4, 5, 6), 70);
  transform3d.Scale3d(7, 8, 9);
  transform3d.Skew(30, 70);

  const Point3F point_3f(14, 15, 16);
  Point3F transformed_point_3f = transform3d.MapPoint(point_3f);
  const std::optional<Point3F> reverted_point_3f =
      transform3d.InverseMapPoint(transformed_point_3f);
  ASSERT_TRUE(reverted_point_3f.has_value());
  EXPECT_TRUE(PointsAreNearlyEqual(reverted_point_3f.value(), point_3f));

  // MapPoint with simple 2d transform.
  transform = Transform::MakeTranslation(10, 20) * Transform::MakeScale(3, 4);
  EXPECT_EQ(PointF(47.5f, 158.0f), transform.MapPoint(PointF(12.5f, 34.5f)));
  EXPECT_EQ(Point3F(47.5f, 158.0f, 56.5f),
            transform.MapPoint(Point3F(12.5f, 34.5f, 56.5f)));

  transform.EnsureFullMatrixForTesting();
  EXPECT_EQ(PointF(47.5f, 158.0f), transform.MapPoint(PointF(12.5f, 34.5f)));
  EXPECT_EQ(Point3F(47.5f, 158.0f, 56.5f),
            transform.MapPoint(Point3F(12.5f, 34.5f, 56.5f)));
}

TEST(XFormTest, MapVector) {
  Transform transform;
  transform.Scale3d(3, 4, 5);
  Vector3dF vector(12.5f, 34.5f, 56.5f);
  Vector3dF expected(37.5f, 138.0f, 282.5f);
  EXPECT_EQ(expected, transform.MapVector(vector));

  // The translation components should be ignored.
  transform.Translate3d(1.25f, 2.75f, 3.875f);
  EXPECT_EQ(expected, transform.MapVector(vector));

  // The perspective components should be ignored.
  transform.set_rc(3, 0, 0.5f);
  transform.set_rc(3, 1, 2.5f);
  transform.set_rc(3, 2, 4.5f);
  transform.set_rc(3, 3, 8.5f);
  EXPECT_EQ(expected, transform.MapVector(vector));

  // MapVector with a simple 2d transform.
  transform = Transform::MakeTranslation(10, 20) * Transform::MakeScale(3, 4);
  expected.set_z(vector.z());
  EXPECT_EQ(expected, transform.MapVector(vector));

  transform.EnsureFullMatrixForTesting();
  EXPECT_EQ(expected, transform.MapVector(vector));
}

TEST(XFormTest, PreConcatAxisTransform2d) {
  auto t = Transform::RowMajor(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                               16, 17);
  auto axis = AxisTransform2d::FromScaleAndTranslation(Vector2dF(10, 20),
                                                       Vector2dF(100, 200));
  auto axis_full =
      Transform::MakeTranslation(100, 200) * Transform::MakeScale(10, 20);
  auto t1 = t;
  t.PreConcat(axis);
  t1.PreConcat(axis_full);
  EXPECT_EQ(t, t1);
}

TEST(XFormTest, PostConcatAxisTransform2d) {
  auto t = Transform::RowMajor(2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                               16, 17);
  auto axis = AxisTransform2d::FromScaleAndTranslation(Vector2dF(10, 20),
                                                       Vector2dF(100, 200));
  auto axis_full =
      Transform::MakeTranslation(100, 200) * Transform::MakeScale(10, 20);
  auto t1 = t;
  t.PostConcat(axis);
  t1.PostConcat(axis_full);
  EXPECT_EQ(t, t1);
}

TEST(XFormTest, ClampOutput) {
  double entries[][2] = {
      // The first entry is used to initialize the transform.
      // The second entry is used to initialize the object to be mapped.
      {std::numeric_limits<float>::max(),
       std::numeric_limits<float>::infinity()},
      {1, std::numeric_limits<float>::infinity()},
      {-1, std::numeric_limits<float>::infinity()},
      {1, -std::numeric_limits<float>::infinity()},
      {
          std::numeric_limits<float>::max(),
          std::numeric_limits<float>::max(),
      },
      {
          std::numeric_limits<float>::lowest(),
          -std::numeric_limits<float>::infinity(),
      },
  };

  for (double* entry : entries) {
    const float mv = entry[0];
    const float factor = entry[1];

    auto is_valid_point = [&](const PointF& p) -> bool {
      return std::isfinite(p.x()) && std::isfinite(p.y());
    };
    auto is_valid_point3 = [&](const Point3F& p) -> bool {
      return std::isfinite(p.x()) && std::isfinite(p.y()) &&
             std::isfinite(p.z());
    };
    auto is_valid_vector2 = [&](const Vector2dF& v) -> bool {
      return std::isfinite(v.x()) && std::isfinite(v.y());
    };
    auto is_valid_vector3 = [&](const Vector3dF& v) -> bool {
      return std::isfinite(v.x()) && std::isfinite(v.y()) &&
             std::isfinite(v.z());
    };
    auto is_valid_rect = [&](const RectF& r) -> bool {
      return is_valid_point(r.origin()) && std::isfinite(r.width()) &&
             std::isfinite(r.height());
    };
    auto is_valid_array = [&](const float* a, size_t size) -> bool {
      for (size_t i = 0; i < size; i++) {
        if (!std::isfinite(a[i]))
          return false;
      }
      return true;
    };

    auto test = [&](const Transform& m) {
      SCOPED_TRACE(base::StringPrintf("m: %s factor: %lg", m.ToString().c_str(),
                                      factor));
      auto p = m.MapPoint(PointF(factor, factor));
      EXPECT_TRUE(is_valid_point(p)) << p.ToString();

      auto p3 = m.MapPoint(Point3F(factor, factor, factor));
      EXPECT_TRUE(is_valid_point3(p3)) << p3.ToString();

      auto r = m.MapRect(RectF(factor, factor, factor, factor));
      EXPECT_TRUE(is_valid_rect(r)) << r.ToString();

      auto v3 = m.MapVector(Vector3dF(factor, factor, factor));
      EXPECT_TRUE(is_valid_vector3(v3)) << v3.ToString();

      float v4[4] = {factor, factor, factor, factor};
      m.TransformVector4(v4);
      EXPECT_TRUE(is_valid_array(v4, 4));

      auto v2 = m.To2dTranslation();
      EXPECT_TRUE(is_valid_vector2(v2)) << v2.ToString();
      v2 = m.To2dScale();
      EXPECT_TRUE(is_valid_vector2(v2)) << v2.ToString();

      v3 = m.To3dTranslation();
      EXPECT_TRUE(is_valid_vector3(v3)) << v3.ToString();
    };

    test(Transform::ColMajor(mv, mv, mv, mv, mv, mv, mv, mv, mv, mv, mv, mv, mv,
                             mv, mv, mv));
    test(Transform::MakeTranslation(mv, mv));
  }
}

constexpr float kProjectionClampedBigNumber =
    1 << (std::numeric_limits<float>::digits - 1);

// This test also demonstrates the relationship between ProjectPoint() and
// MapPoint().
TEST(XFormTest, ProjectPoint) {
  Transform transform;
  PointF p(1.25f, -3.5f);
  bool clamped = true;
  EXPECT_EQ(p, transform.ProjectPoint(p));
  EXPECT_EQ(p, transform.ProjectPoint(p, &clamped));
  EXPECT_FALSE(clamped);
  // MapPoint() and ProjectPoint() are the same with a flat transform.
  EXPECT_EQ(p, transform.MapPoint(p));

  // ProjectPoint with simple 2d transform.
  transform = Transform::MakeTranslation(10, 20) * Transform::MakeScale(3, 4);
  clamped = true;
  gfx::PointF projected = transform.ProjectPoint(p, &clamped);
  EXPECT_EQ(PointF(13.75f, 6.f), projected);
  EXPECT_FALSE(clamped);
  // MapPoint() and ProjectPoint() are the same with a flat transform.
  EXPECT_EQ(projected, transform.MapPoint(p));

  clamped = true;
  transform.EnsureFullMatrixForTesting();
  EXPECT_EQ(projected, transform.ProjectPoint(p, &clamped));
  EXPECT_FALSE(clamped);
  EXPECT_EQ(projected, transform.MapPoint(p));

  // Set scale z to 0.
  transform.set_rc(2, 2, 0);
  clamped = true;
  projected = transform.ProjectPoint(p, &clamped);
  EXPECT_EQ(PointF(), projected);
  EXPECT_TRUE(clamped);
  // MapPoint() still produces the original result.
  EXPECT_EQ(PointF(13.75f, 6.f), transform.MapPoint(p));

  // Normally (except the last case below), t.ProjectPoint() is equivalent to
  // inverse(flatten(inverse(t))).MapPoint().
  auto projection_transform = [](const Transform& t) {
    auto flat = t.GetCheckedInverse();
    flat.Flatten();
    return flat.GetCheckedInverse();
  };

  transform.MakeIdentity();
  transform.RotateAboutYAxis(60);
  clamped = true;
  projected = transform.ProjectPoint(p, &clamped);
  EXPECT_EQ(PointF(2.5f, -3.5f), projected);
  EXPECT_FALSE(clamped);
  EXPECT_EQ(PointF(0.625f, -3.5f), transform.MapPoint(p));

  EXPECT_EQ(projected, projection_transform(transform).MapPoint(p));
  EXPECT_EQ(projected, projection_transform(transform).ProjectPoint(p));

  transform.ApplyPerspectiveDepth(10);
  clamped = true;
  projected = transform.ProjectPoint(p, &clamped);
  EXPECT_POINTF_NEAR(PointF(3.19f, -4.47f), projected, 0.01f);
  EXPECT_FALSE(clamped);
  EXPECT_EQ(PointF(0.625f, -3.5f), transform.MapPoint(p));

  EXPECT_POINTF_NEAR(projected, projection_transform(transform).MapPoint(p),
                     1e-5f);
  EXPECT_POINTF_NEAR(projected, projection_transform(transform).ProjectPoint(p),
                     1e-5f);

  // With a small perspective, the ray doesn't intersect the destination plane.
  transform.ApplyPerspectiveDepth(2);
  clamped = false;
  projected = transform.ProjectPoint(p, &clamped);
  EXPECT_TRUE(clamped);
  EXPECT_EQ(projected.x(), kProjectionClampedBigNumber);
  EXPECT_EQ(projected.y(), -kProjectionClampedBigNumber);
  EXPECT_EQ(PointF(0.625f, -3.5f), transform.MapPoint(p));
  // In this case, MapPoint() returns a point behind the eye.
  EXPECT_POINTF_NEAR(PointF(-8.36014f, 11.7042f),
                     projection_transform(transform).MapPoint(p), 1e-5f);
  EXPECT_POINTF_NEAR(projected, projection_transform(transform).ProjectPoint(p),
                     1e-5f);
}

TEST(XFormTest, ProjectQuad) {
  auto transform = Transform::MakeTranslation(3.25f, 7.75f);
  QuadF q(PointF(1.25f, 2.5f), PointF(3.75f, 4.f), PointF(23.f, 45.f),
          PointF(12.f, 67.f));
  EXPECT_EQ(QuadF(PointF(4.5f, 10.25f), PointF(7.f, 11.75f),
                  PointF(26.25f, 52.75f), PointF(15.25f, 74.75f)),
            transform.ProjectQuad(q));

  transform.set_rc(2, 2, 0);
  EXPECT_EQ(QuadF(), transform.ProjectQuad(q));

  transform.MakeIdentity();
  transform.RotateAboutYAxis(60);
  EXPECT_EQ(QuadF(PointF(2.5f, 2.5f), PointF(7.5f, 4.f), PointF(46.f, 45.f),
                  PointF(24.f, 67.f)),
            transform.ProjectQuad(q));

  // With a small perspective, all points of |q| are clamped, and the
  // projected result is an empty quad.
  transform.ApplyPerspectiveDepth(2);
  EXPECT_EQ(QuadF(), transform.ProjectQuad(q));

  // Change the quad so that 2 points are clamped.
  q.set_p1(PointF(-1.25f, -2.5f));
  q.set_p2(PointF(-3.75f, 4.f));
  q.set_p3(PointF(23.f, -45.f));
  QuadF q1 = transform.ProjectQuad(q);
  EXPECT_POINTF_NEAR(PointF(-1.2f, -1.2f), q1.p1(), 0.01f);
  EXPECT_POINTF_NEAR(PointF(-1.77f, 0.94f), q1.p2(), 0.01f);
  EXPECT_EQ(q1.p3().x(), kProjectionClampedBigNumber);
  EXPECT_EQ(q1.p3().y(), -kProjectionClampedBigNumber);
  EXPECT_EQ(q1.p4().x(), kProjectionClampedBigNumber);
  EXPECT_EQ(q1.p4().y(), kProjectionClampedBigNumber);
}

TEST(XFormTest, ToString) {
  auto zeros =
      Transform::ColMajor(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ("[ 0 0 0 0\n  0 0 0 0\n  0 0 0 0\n  0 0 0 0 ]\n", zeros.ToString());
  EXPECT_EQ("[ 0 0 0 0\n  0 0 0 0\n  0 0 0 0\n  0 0 0 0 ]\n(degenerate)",
            zeros.ToDecomposedString());

  Transform identity;
  EXPECT_EQ("[ 1 0 0 0\n  0 1 0 0\n  0 0 1 0\n  0 0 0 1 ]\n",
            identity.ToString());
  EXPECT_EQ("identity", identity.ToDecomposedString());

  Transform translation;
  translation.Translate3d(3, 5, 7);
  EXPECT_EQ("[ 1 0 0 3\n  0 1 0 5\n  0 0 1 7\n  0 0 0 1 ]\n",
            translation.ToString());
  EXPECT_EQ("translate: 3,5,7", translation.ToDecomposedString());

  auto transform = Transform::ColMajor(1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7, 8.8,
                                       1e20, 1e-20, 1.0 / 3.0, 0, 0, 0, 0, 1);
  EXPECT_EQ(
      "[ 1.1 5.5 1e+20 0\n  2.2 6.6 1e-20 0\n  3.3 7.7 0.333333 0\n"
      "  4.4 8.8 0 1 ]\n",
      transform.ToString());
  EXPECT_EQ(
      "translate: +0 +0 +0\n"
      "scale: -4.11582 -2.88048 -4.08248e+19\n"
      "skew: +3.87836 +0.654654 +2.13809\n"
      "perspective: -6.66667e-21 -1 +2 +1\n"
      "quaternion: -0.582925 +0.603592 +0.518949 +0.162997\n",
      transform.ToDecomposedString());
}

TEST(XFormTest, Is2dProportionalUpscaleAndOr2dTranslation) {
  Transform transform;
  EXPECT_TRUE(transform.Is2dProportionalUpscaleAndOr2dTranslation());

  transform.MakeIdentity();
  transform.Translate(10, 0);
  EXPECT_TRUE(transform.Is2dProportionalUpscaleAndOr2dTranslation());

  transform.MakeIdentity();
  transform.Scale(1.3);
  EXPECT_TRUE(transform.Is2dProportionalUpscaleAndOr2dTranslation());

  transform.MakeIdentity();
  transform.Translate(0, -20);
  transform.Scale(1.7);
  EXPECT_TRUE(transform.Is2dProportionalUpscaleAndOr2dTranslation());

  transform.MakeIdentity();
  transform.Scale(0.99);
  EXPECT_FALSE(transform.Is2dProportionalUpscaleAndOr2dTranslation());

  transform.MakeIdentity();
  transform.Translate3d(0, 0, 1);
  EXPECT_FALSE(transform.Is2dProportionalUpscaleAndOr2dTranslation());

  transform.MakeIdentity();
  transform.Rotate(40);
  EXPECT_FALSE(transform.Is2dProportionalUpscaleAndOr2dTranslation());

  transform.MakeIdentity();
  transform.SkewX(30);
  EXPECT_FALSE(transform.Is2dProportionalUpscaleAndOr2dTranslation());
}

TEST(XFormTest, Creates3d) {
  EXPECT_FALSE(Transform().Creates3d());
  EXPECT_FALSE(Transform::MakeTranslation(1, 2).Creates3d());

  Transform transform;
  transform.ApplyPerspectiveDepth(100);
  EXPECT_FALSE(transform.Creates3d());
  transform.Scale3d(2, 3, 4);
  EXPECT_FALSE(transform.Creates3d());
  transform.Translate3d(1, 2, 3);
  EXPECT_TRUE(transform.Creates3d());

  transform.MakeIdentity();
  transform.RotateAboutYAxis(20);
  EXPECT_TRUE(transform.Creates3d());
}

TEST(XFormTest, ApplyTransformOrigin) {
  // (0,0,0) is a fixed point of this scale.
  // (1,1,1) should be scaled appropriately.
  Transform transform;
  transform.Scale3d(2, 3, 4);
  EXPECT_EQ(Point3F(0, 0, 0), transform.MapPoint(Point3F(0, 0, 0)));
  EXPECT_EQ(Point3F(2, 3, -4), transform.MapPoint(Point3F(1, 1, -1)));

  // With the transform origin applied, (1,2,3) is the fixed point.
  // (0,0,0) should be scaled according to its distance from (1,2,3).
  transform.ApplyTransformOrigin(1, 2, 3);
  EXPECT_EQ(Point3F(1, 2, 3), transform.MapPoint(Point3F(1, 2, 3)));
  EXPECT_EQ(Point3F(-1, -4, -9), transform.MapPoint(Point3F(0, 0, 0)));

  transform = GetTestMatrix1();
  Vector3dF origin(5.f, 6.f, 7.f);
  Transform with_origin = transform;
  Point3F p(41.f, 43.f, 47.f);
  with_origin.ApplyTransformOrigin(origin.x(), origin.y(), origin.z());
  EXPECT_POINT3F_EQ(transform.MapPoint(p - origin) + origin,
                    with_origin.MapPoint(p));
}

TEST(XFormTest, Zoom) {
  Transform transform = GetTestMatrix1();
  auto zoomed = transform;
  zoomed.Zoom(2.f);
  Point3F p(41.f, 43.f, 47.f);
  Point3F expected = p;
  expected.Scale(0.5f, 0.5f, 0.5f);
  expected = transform.MapPoint(expected);
  expected.Scale(2.f, 2.f, 2.f);
  EXPECT_POINT3F_EQ(expected, zoomed.MapPoint(p));
}

TEST(XFormTest, ApproximatelyEqual) {
  EXPECT_TRUE(Transform().ApproximatelyEqual(Transform()));
  EXPECT_TRUE(Transform().ApproximatelyEqual(Transform(), 0));
  EXPECT_TRUE(GetTestMatrix1().ApproximatelyEqual(GetTestMatrix1()));
  EXPECT_TRUE(GetTestMatrix1().ApproximatelyEqual(GetTestMatrix1(), 0));

  Transform t1 = Transform::MakeTranslation(0.9, -0.9);
  Transform t2 = Transform::MakeScale(1.099, 0.901);
  EXPECT_TRUE(t1.ApproximatelyEqual(t2));
  EXPECT_FALSE(t1.ApproximatelyEqual(t2, 0.8f, 0.2f, 0.0f));
  EXPECT_FALSE(t1.ApproximatelyEqual(t2, 1.0f, 0.01f, 0.0f));
  EXPECT_FALSE(t1.ApproximatelyEqual(t2, 1.0f, 0.01f, 0.05f));
  EXPECT_TRUE(t1.ApproximatelyEqual(t2, 1.0f, 0.2f, 1.f));
  EXPECT_TRUE(t1.ApproximatelyEqual(t2, 1.0f, 0.2f, 0.1f));

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 4; c++) {
      t1 = Transform();
      t1.set_rc(r, c, t1.rc(r, c) + 0.25f);
      EXPECT_TRUE(t1.ApproximatelyEqual(Transform(), 0.25f));
      EXPECT_FALSE(t1.ApproximatelyEqual(Transform(), 0.24f));
    }
  }
}

}  // namespace

}  // namespace gfx
