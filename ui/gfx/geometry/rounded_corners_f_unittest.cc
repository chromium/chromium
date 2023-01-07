// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/rounded_corners_f.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(RoundedCornersFTest, DefaultConstructor) {
  const RoundedCornersF rc;
  EXPECT_EQ(0.0f, rc.upper_left());
  EXPECT_EQ(0.0f, rc.upper_right());
  EXPECT_EQ(0.0f, rc.lower_right());
  EXPECT_EQ(0.0f, rc.lower_left());
}

TEST(RoundedCornersFTest, FromSingleValue) {
  constexpr float kValue = 1.33f;
  const RoundedCornersF rc(kValue);
  EXPECT_EQ(kValue, rc.upper_left());
  EXPECT_EQ(kValue, rc.upper_right());
  EXPECT_EQ(kValue, rc.lower_right());
  EXPECT_EQ(kValue, rc.lower_left());
}

TEST(RoundedCornersFTest, FromFourValues) {
  constexpr float kValue1 = 1.33f;
  constexpr float kValue2 = 2.66f;
  constexpr float kValue3 = 0.1f;
  constexpr float kValue4 = 50.0f;
  const RoundedCornersF rc(kValue1, kValue2, kValue3, kValue4);
  EXPECT_EQ(kValue1, rc.upper_left());
  EXPECT_EQ(kValue2, rc.upper_right());
  EXPECT_EQ(kValue3, rc.lower_right());
  EXPECT_EQ(kValue4, rc.lower_left());
}

TEST(RoundedCornersFTest, IsEmpty) {
  EXPECT_TRUE(RoundedCornersF().IsEmpty());
  EXPECT_FALSE(RoundedCornersF(1.0f).IsEmpty());
  EXPECT_FALSE(RoundedCornersF(1.0f, 0.0f, 0.0f, 0.0f).IsEmpty());
  EXPECT_FALSE(RoundedCornersF(0.0f, 1.0f, 0.0f, 0.0f).IsEmpty());
  EXPECT_FALSE(RoundedCornersF(0.0f, 0.0f, 1.0f, 0.0f).IsEmpty());
  EXPECT_FALSE(RoundedCornersF(0.0f, 0.0f, 0.0f, 1.0f).IsEmpty());
}

TEST(RoundedCornersFTest, Equality) {
  constexpr RoundedCornersF kCorners(1.33f, 2.66f, 0.1f, 50.0f);
  RoundedCornersF rc = kCorners;
  // Using EXPECT_TRUE and EXPECT_FALSE to explicitly test == and != operators
  // (rather than EXPECT_EQ, EXPECT_NE).
  EXPECT_TRUE(rc == kCorners);
  EXPECT_FALSE(rc != kCorners);
  rc.set_upper_left(2.0f);
  EXPECT_FALSE(rc == kCorners);
  EXPECT_TRUE(rc != kCorners);
  rc = kCorners;
  rc.set_upper_right(2.0f);
  EXPECT_FALSE(rc == kCorners);
  EXPECT_TRUE(rc != kCorners);
  rc = kCorners;
  rc.set_lower_left(2.0f);
  EXPECT_FALSE(rc == kCorners);
  EXPECT_TRUE(rc != kCorners);
  rc = kCorners;
  rc.set_lower_right(2.0f);
  EXPECT_FALSE(rc == kCorners);
  EXPECT_TRUE(rc != kCorners);
}

TEST(RoundedCornersFTest, Set) {
  RoundedCornersF rc(1.0f, 2.0f, 3.0f, 4.0f);
  rc.Set(4.0f, 3.0f, 2.0f, 1.0f);
  EXPECT_EQ(4.0f, rc.upper_left());
  EXPECT_EQ(3.0f, rc.upper_right());
  EXPECT_EQ(2.0f, rc.lower_right());
  EXPECT_EQ(1.0f, rc.lower_left());
}

TEST(RoundedCornersFTest, SetProperties) {
  RoundedCornersF rc(1.0f, 2.0f, 3.0f, 4.0f);

  rc.set_upper_left(50.0f);
  EXPECT_EQ(50.0f, rc.upper_left());
  EXPECT_EQ(2.0f, rc.upper_right());
  EXPECT_EQ(3.0f, rc.lower_right());
  EXPECT_EQ(4.0f, rc.lower_left());

  rc.set_upper_right(40.0f);
  EXPECT_EQ(50.0f, rc.upper_left());
  EXPECT_EQ(40.0f, rc.upper_right());
  EXPECT_EQ(3.0f, rc.lower_right());
  EXPECT_EQ(4.0f, rc.lower_left());

  rc.set_lower_right(30.0f);
  EXPECT_EQ(50.0f, rc.upper_left());
  EXPECT_EQ(40.0f, rc.upper_right());
  EXPECT_EQ(30.0f, rc.lower_right());
  EXPECT_EQ(4.0f, rc.lower_left());

  rc.set_lower_left(20.0f);
  EXPECT_EQ(50.0f, rc.upper_left());
  EXPECT_EQ(40.0f, rc.upper_right());
  EXPECT_EQ(30.0f, rc.lower_right());
  EXPECT_EQ(20.0f, rc.lower_left());
}

namespace {

// Verify that IsEmpty() returns true and that all values are exactly zero.
void VerifyEmptyAndZero(const RoundedCornersF& rc) {
  EXPECT_TRUE(rc.IsEmpty());
  EXPECT_EQ(0.0f, rc.upper_left());
  EXPECT_EQ(0.0f, rc.upper_right());
  EXPECT_EQ(0.0f, rc.lower_right());
  EXPECT_EQ(0.0f, rc.lower_left());
}

}  // namespace

TEST(RoundedCornersFTest, Epsilon) {
  constexpr float kEpsilon = std::numeric_limits<float>::epsilon();
  RoundedCornersF rc(kEpsilon, kEpsilon, kEpsilon, kEpsilon);
  VerifyEmptyAndZero(rc);

  rc.set_upper_left(kEpsilon);
  VerifyEmptyAndZero(rc);
  rc.set_upper_right(kEpsilon);
  VerifyEmptyAndZero(rc);
  rc.set_lower_right(kEpsilon);
  VerifyEmptyAndZero(rc);
  rc.set_lower_left(kEpsilon);
  VerifyEmptyAndZero(rc);

  rc.Set(kEpsilon, kEpsilon, kEpsilon, kEpsilon);
  VerifyEmptyAndZero(rc);
}

TEST(RoundedCornersFTest, Negative) {
  constexpr float kNegative = -0.5f;
  RoundedCornersF rc(kNegative, kNegative, kNegative, kNegative);
  VerifyEmptyAndZero(rc);

  rc.set_upper_left(kNegative);
  VerifyEmptyAndZero(rc);
  rc.set_upper_right(kNegative);
  VerifyEmptyAndZero(rc);
  rc.set_lower_right(kNegative);
  VerifyEmptyAndZero(rc);
  rc.set_lower_left(kNegative);
  VerifyEmptyAndZero(rc);

  rc.Set(kNegative, kNegative, kNegative, kNegative);
  VerifyEmptyAndZero(rc);
}

}  // namespace gfx
