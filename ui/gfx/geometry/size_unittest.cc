// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/size.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(SizeTest, SetToMinMax) {
  Size a;

  a = Size(3, 5);
  EXPECT_EQ(Size(3, 5).ToString(), a.ToString());
  a.SetToMax(Size(2, 4));
  EXPECT_EQ(Size(3, 5).ToString(), a.ToString());
  a.SetToMax(Size(3, 5));
  EXPECT_EQ(Size(3, 5).ToString(), a.ToString());
  a.SetToMax(Size(4, 2));
  EXPECT_EQ(Size(4, 5).ToString(), a.ToString());
  a.SetToMax(Size(8, 10));
  EXPECT_EQ(Size(8, 10).ToString(), a.ToString());

  a.SetToMin(Size(9, 11));
  EXPECT_EQ(Size(8, 10).ToString(), a.ToString());
  a.SetToMin(Size(8, 10));
  EXPECT_EQ(Size(8, 10).ToString(), a.ToString());
  a.SetToMin(Size(11, 9));
  EXPECT_EQ(Size(8, 9).ToString(), a.ToString());
  a.SetToMin(Size(7, 11));
  EXPECT_EQ(Size(7, 9).ToString(), a.ToString());
  a.SetToMin(Size(3, 5));
  EXPECT_EQ(Size(3, 5).ToString(), a.ToString());
}

TEST(SizeTest, Enlarge) {
  Size test(3, 4);
  test.Enlarge(5, -8);
  EXPECT_EQ(test, Size(8, -4));
}

TEST(SizeTest, IntegerOverflow) {
  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  Size max_size(int_max, int_max);
  Size min_size(int_min, int_min);
  Size test;

  test = Size();
  test.Enlarge(int_max, int_max);
  EXPECT_EQ(test, max_size);

  test = Size();
  test.Enlarge(int_min, int_min);
  EXPECT_EQ(test, min_size);

  test = Size(10, 20);
  test.Enlarge(int_max, int_max);
  EXPECT_EQ(test, max_size);

  test = Size(-10, -20);
  test.Enlarge(int_min, int_min);
  EXPECT_EQ(test, min_size);
}

TEST(SizeTest, OperatorAddSub) {
  Size lhs(100, 20);
  Size rhs(50, 10);

  lhs += rhs;
  EXPECT_EQ(Size(150, 30), lhs);

  lhs = Size(100, 20);
  EXPECT_EQ(Size(150, 30), lhs + rhs);

  lhs = Size(100, 20);
  lhs -= rhs;
  EXPECT_EQ(Size(50, 10), lhs);

  lhs = Size(100, 20);
  EXPECT_EQ(Size(50, 10), lhs - rhs);
}

TEST(SizeTest, OperatorAddOverflow) {
  int int_max = std::numeric_limits<int>::max();

  Size lhs(int_max, int_max);
  Size rhs(int_max, int_max);
  EXPECT_EQ(Size(int_max, int_max), lhs + rhs);
}

TEST(SizeTest, OperatorSubClampAtZero) {
  Size lhs(10, 10);
  Size rhs(100, 100);
  EXPECT_EQ(Size(0, 0), lhs - rhs);

  lhs = Size(10, 10);
  rhs = Size(100, 100);
  lhs -= rhs;
  EXPECT_EQ(Size(0, 0), lhs);
}

TEST(SizeTest, OperatorCompare) {
  Size lhs(100, 20);
  Size rhs(50, 10);

  EXPECT_TRUE(lhs != rhs);
  EXPECT_FALSE(lhs == rhs);

  rhs = Size(100, 20);
  EXPECT_TRUE(lhs == rhs);
  EXPECT_FALSE(lhs != rhs);
}

TEST(SizeTest, Transpose) {
  gfx::Size s(1, 2);
  EXPECT_EQ(gfx::Size(2, 1), TransposeSize(s));
  s.Transpose();
  EXPECT_EQ(gfx::Size(2, 1), s);
}

}  // namespace gfx
