// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/geometry/quaternion.h"

#include <cmath>
#include <numbers>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace gfx {

namespace {

const double kEpsilon = 1e-7;

#define EXPECT_QUATERNION(expected, actual)          \
  do {                                               \
    EXPECT_NEAR(expected.x(), actual.x(), kEpsilon); \
    EXPECT_NEAR(expected.y(), actual.y(), kEpsilon); \
    EXPECT_NEAR(expected.z(), actual.z(), kEpsilon); \
    EXPECT_NEAR(expected.w(), actual.w(), kEpsilon); \
  } while (false)

void CompareQuaternions(const Quaternion& a, const Quaternion& b) {
  EXPECT_FLOAT_EQ(a.x(), b.x());
  EXPECT_FLOAT_EQ(a.y(), b.y());
  EXPECT_FLOAT_EQ(a.z(), b.z());
  EXPECT_FLOAT_EQ(a.w(), b.w());
}

}  // namespace

TEST(QuatTest, DefaultConstruction) {
  CompareQuaternions(Quaternion(0, 0, 0, 1), Quaternion());
}

TEST(QuatTest, AxisAngleCommon) {
  double radians = 0.5;
  Quaternion q(Vector3dF(1, 0, 0), radians);
  CompareQuaternions(
      Quaternion(std::sin(radians / 2), 0, 0, std::cos(radians / 2)), q);
}

TEST(QuatTest, VectorToVectorRotation) {
  Quaternion q(Vector3dF(1.0f, 0.0f, 0.0f), Vector3dF(0.0f, 1.0f, 0.0f));
  Quaternion r(Vector3dF(0.0f, 0.0f, 1.0f), std::numbers::pi_v<float> / 2);

  EXPECT_FLOAT_EQ(r.x(), q.x());
  EXPECT_FLOAT_EQ(r.y(), q.y());
  EXPECT_FLOAT_EQ(r.z(), q.z());
  EXPECT_FLOAT_EQ(r.w(), q.w());
}

TEST(QuatTest, AxisAngleWithZeroLengthAxis) {
  Quaternion q(Vector3dF(0, 0, 0), 0.5);
  // If the axis of zero length, we should assume the default values.
  CompareQuaternions(q, Quaternion());
}

TEST(QuatTest, Addition) {
  double values[] = {0, 1, 100};
  for (size_t i = 0; i < std::size(values); ++i) {
    float t = values[i];
    Quaternion a(t, 2 * t, 3 * t, 4 * t);
    Quaternion b(5 * t, 4 * t, 3 * t, 2 * t);
    Quaternion sum = a + b;
    CompareQuaternions(Quaternion(t, t, t, t) * 6, sum);
  }
}

TEST(QuatTest, Multiplication) {
  struct {
    Quaternion a;
    Quaternion b;
    Quaternion expected;
  } cases[] = {
      {Quaternion(1, 0, 0, 0), Quaternion(1, 0, 0, 0), Quaternion(0, 0, 0, -1)},
      {Quaternion(0, 1, 0, 0), Quaternion(0, 1, 0, 0), Quaternion(0, 0, 0, -1)},
      {Quaternion(0, 0, 1, 0), Quaternion(0, 0, 1, 0), Quaternion(0, 0, 0, -1)},
      {Quaternion(0, 0, 0, 1), Quaternion(0, 0, 0, 1), Quaternion(0, 0, 0, 1)},
      {Quaternion(1, 2, 3, 4), Quaternion(5, 6, 7, 8),
       Quaternion(24, 48, 48, -6)},
      {Quaternion(5, 6, 7, 8), Quaternion(1, 2, 3, 4),
       Quaternion(32, 32, 56, -6)},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    Quaternion product = cases[i].a * cases[i].b;
    CompareQuaternions(cases[i].expected, product);
  }
}

TEST(QuatTest, Scaling) {
  double values[] = {0, 10, 100};
  for (size_t i = 0; i < std::size(values); ++i) {
    double s = values[i];
    Quaternion q(1, 2, 3, 4);
    Quaternion expected(s, 2 * s, 3 * s, 4 * s);
    CompareQuaternions(expected, q * s);
    CompareQuaternions(expected, s * q);
    if (s > 0)
      CompareQuaternions(expected, q / (1 / s));
  }
}

TEST(QuatTest, Normalization) {
  Quaternion q(1, -1, 1, -1);
  EXPECT_NEAR(q.Length(), 4, kEpsilon);

  q = q.Normalized();

  EXPECT_NEAR(q.Length(), 1, kEpsilon);
  EXPECT_NEAR(q.x(), 0.5, kEpsilon);
  EXPECT_NEAR(q.y(), -0.5, kEpsilon);
  EXPECT_NEAR(q.z(), 0.5, kEpsilon);
  EXPECT_NEAR(q.w(), -0.5, kEpsilon);
}

TEST(QuatTest, Lerp) {
  for (size_t i = 1; i < 100; ++i) {
    Quaternion a(0, 0, 0, 0);
    Quaternion b(1, 2, 3, 4);
    float t = static_cast<float>(i) / 100.0f;
    Quaternion interpolated = a.Lerp(b, t);
    double s = 1.0 / sqrt(30.0);
    CompareQuaternions(Quaternion(1, 2, 3, 4) * s, interpolated);
  }

  Quaternion a(4, 3, 2, 1);
  Quaternion b(1, 2, 3, 4);
  CompareQuaternions(a.Normalized(), a.Lerp(b, 0));
  CompareQuaternions(b.Normalized(), a.Lerp(b, 1));
  CompareQuaternions(Quaternion(1, 1, 1, 1).Normalized(), a.Lerp(b, 0.5));
}

TEST(QuatTest, Slerp) {
  Vector3dF axis(1, 1, 1);
  double start_radians = -0.5;
  double stop_radians = 0.5;
  Quaternion start(axis, start_radians);
  Quaternion stop(axis, stop_radians);

  for (size_t i = 0; i < 100; ++i) {
    float t = static_cast<float>(i) / 100.0f;
    double radians = (1.0 - t) * start_radians + t * stop_radians;
    Quaternion expected(axis, radians);
    Quaternion interpolated = start.Slerp(stop, t);
    EXPECT_QUATERNION(expected, interpolated);
  }
}

TEST(QuatTest, SlerpOppositeAngles) {
  Vector3dF axis(1, 1, 1);
  double start_radians = -std::numbers::pi / 2;
  double stop_radians = std::numbers::pi / 2;
  Quaternion start(axis, start_radians);
  Quaternion stop(axis, stop_radians);

  // When quaternions are pointed in the fully opposite direction, this is
  // ambiguous, so we rotate as per https://www.w3.org/TR/css-transforms-1/
  Quaternion expected(axis, 0);

  Quaternion interpolated = start.Slerp(stop, 0.5f);
  EXPECT_QUATERNION(expected, interpolated);
}

TEST(QuatTest, SlerpRotateXRotateY) {
  Quaternion start(Vector3dF(1, 0, 0), std::numbers::pi / 2);
  Quaternion stop(Vector3dF(0, 1, 0), std::numbers::pi / 2);
  Quaternion interpolated = start.Slerp(stop, 0.5f);

  double expected_angle = std::acos(1.0 / 3.0);
  double xy = std::sin(0.5 * expected_angle) / std::sqrt(2);
  Quaternion expected(xy, xy, 0, std::cos(0.5 * expected_angle));
  EXPECT_QUATERNION(expected, interpolated);
}

TEST(QuatTest, Slerp360) {
  Quaternion start(0, 0, 0, -1);  // 360 degree rotation.
  Quaternion stop(Vector3dF(0, 0, 1), std::numbers::pi / 2);
  Quaternion interpolated = start.Slerp(stop, 0.5f);
  double expected_half_angle = std::numbers::pi / 8;
  Quaternion expected(0, 0, std::sin(expected_half_angle),
                      std::cos(expected_half_angle));
  EXPECT_QUATERNION(expected, interpolated);
}

TEST(QuatTest, SlerpEquivalentQuaternions) {
  Quaternion start(Vector3dF(1, 0, 0), std::numbers::pi / 3);
  Quaternion stop = start.flip();
  Quaternion interpolated = start.Slerp(stop, 0.5f);
  EXPECT_QUATERNION(start, interpolated);
}

TEST(QuatTest, SlerpQuaternionWithInverse) {
  Quaternion start(Vector3dF(1, 0, 0), std::numbers::pi / 3);
  Quaternion stop = start.inverse();
  Quaternion interpolated = start.Slerp(stop, 0.5f);
  Quaternion expected(0, 0, 0, 1);
  EXPECT_QUATERNION(expected, interpolated);
}

TEST(QuatTest, SlerpObtuseAngle) {
  Quaternion start(Vector3dF(1, 1, 0), std::numbers::pi / 2);
  Quaternion stop(Vector3dF(0, 1, -1), 3 * std::numbers::pi / 2);
  Quaternion interpolated = start.Slerp(stop, 0.5f);
  double expected_half_angle = -std::atan(0.5);
  double xz = std::sin(expected_half_angle) / std::sqrt(2);
  Quaternion expected(xz, 0, xz, -std::cos(expected_half_angle));
  EXPECT_QUATERNION(expected, interpolated);
}

TEST(QuatTest, Equals) {
  EXPECT_TRUE(Quaternion() == Quaternion());
  EXPECT_TRUE(Quaternion() == Quaternion(0, 0, 0, 1));
  EXPECT_TRUE(Quaternion(1, 5.2, -8.5, 222.2) ==
              Quaternion(1, 5.2, -8.5, 222.2));
  EXPECT_FALSE(Quaternion() == Quaternion(1, 0, 0, 0));
  EXPECT_FALSE(Quaternion() == Quaternion(0, 1, 0, 0));
  EXPECT_FALSE(Quaternion() == Quaternion(0, 0, 1, 0));
  EXPECT_FALSE(Quaternion() == Quaternion(1, 0, 0, 1));
}

TEST(QuatTest, NotEquals) {
  EXPECT_FALSE(Quaternion() != Quaternion());
  EXPECT_FALSE(Quaternion(1, 5.2, -8.5, 222.2) !=
               Quaternion(1, 5.2, -8.5, 222.2));
  EXPECT_TRUE(Quaternion() != Quaternion(1, 0, 0, 0));
  EXPECT_TRUE(Quaternion() != Quaternion(0, 1, 0, 0));
  EXPECT_TRUE(Quaternion() != Quaternion(0, 0, 1, 0));
  EXPECT_TRUE(Quaternion() != Quaternion(1, 0, 0, 1));
}

}  // namespace gfx
