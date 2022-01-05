// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/insets.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {

TEST(InsetsTest, Default) {
  Insets insets;
  EXPECT_EQ(0, insets.top());
  EXPECT_EQ(0, insets.left());
  EXPECT_EQ(0, insets.bottom());
  EXPECT_EQ(0, insets.right());
}

TEST(InsetsTest, Insets) {
  Insets insets(1, 2, 3, 4);
  EXPECT_EQ(1, insets.top());
  EXPECT_EQ(2, insets.left());
  EXPECT_EQ(3, insets.bottom());
  EXPECT_EQ(4, insets.right());
}

TEST(InsetsTest, SetTop) {
  Insets insets(1);
  insets.set_top(2);
  EXPECT_EQ(Insets(2, 1, 1, 1), insets);
}

TEST(InsetsTest, SetBottom) {
  Insets insets(1);
  insets.set_bottom(2);
  EXPECT_EQ(Insets(1, 1, 2, 1), insets);
}

TEST(InsetsTest, SetLeft) {
  Insets insets(1);
  insets.set_left(2);
  EXPECT_EQ(Insets(1, 2, 1, 1), insets);
}

TEST(InsetsTest, SetRight) {
  Insets insets(1);
  insets.set_right(2);
  EXPECT_EQ(Insets(1, 1, 1, 2), insets);
}

TEST(InsetsTest, Set) {
  Insets insets;
  insets.Set(1, 2, 3, 4);
  EXPECT_EQ(1, insets.top());
  EXPECT_EQ(2, insets.left());
  EXPECT_EQ(3, insets.bottom());
  EXPECT_EQ(4, insets.right());
}

TEST(InsetsTest, WidthHeightAndIsEmpty) {
  Insets insets;
  EXPECT_EQ(0, insets.width());
  EXPECT_EQ(0, insets.height());
  EXPECT_TRUE(insets.IsEmpty());

  insets.Set(0, 3, 0, 4);
  EXPECT_EQ(7, insets.width());
  EXPECT_EQ(0, insets.height());
  EXPECT_FALSE(insets.IsEmpty());

  insets.Set(1, 0, 2, 0);
  EXPECT_EQ(0, insets.width());
  EXPECT_EQ(3, insets.height());
  EXPECT_FALSE(insets.IsEmpty());

  insets.Set(1, 4, 2, 5);
  EXPECT_EQ(9, insets.width());
  EXPECT_EQ(3, insets.height());
  EXPECT_FALSE(insets.IsEmpty());
}

TEST(InsetsTest, Operators) {
  Insets insets;
  insets.Set(1, 2, 3, 4);
  insets += Insets(5, 6, 7, 8);
  EXPECT_EQ(6, insets.top());
  EXPECT_EQ(8, insets.left());
  EXPECT_EQ(10, insets.bottom());
  EXPECT_EQ(12, insets.right());

  insets -= Insets(-1, 0, 1, 2);
  EXPECT_EQ(7, insets.top());
  EXPECT_EQ(8, insets.left());
  EXPECT_EQ(9, insets.bottom());
  EXPECT_EQ(10, insets.right());

  insets = Insets(10, 10, 10, 10) + Insets(5, 5, 0, -20);
  EXPECT_EQ(15, insets.top());
  EXPECT_EQ(15, insets.left());
  EXPECT_EQ(10, insets.bottom());
  EXPECT_EQ(-10, insets.right());

  insets = Insets(10, 10, 10, 10) - Insets(5, 5, 0, -20);
  EXPECT_EQ(5, insets.top());
  EXPECT_EQ(5, insets.left());
  EXPECT_EQ(10, insets.bottom());
  EXPECT_EQ(30, insets.right());
}

TEST(InsetsTest, Equality) {
  Insets insets1;
  insets1.Set(1, 2, 3, 4);
  Insets insets2;
  // Test operator== and operator!=.
  EXPECT_FALSE(insets1 == insets2);
  EXPECT_TRUE(insets1 != insets2);

  insets2.Set(1, 2, 3, 4);
  EXPECT_TRUE(insets1 == insets2);
  EXPECT_FALSE(insets1 != insets2);
}

TEST(InsetsTest, ToString) {
  Insets insets(1, 2, 3, 4);
  EXPECT_EQ("1,2,3,4", insets.ToString());
}

TEST(InsetsTest, Offset) {
  const Insets insets(1, 2, 3, 4);
  const Rect rect(5, 6, 7, 8);
  const Vector2d vector(9, 10);

  // Whether you inset then offset the rect, offset then inset the rect, or
  // offset the insets then apply to the rect, the outcome should be the same.
  Rect inset_first = rect;
  inset_first.Inset(insets);
  inset_first.Offset(vector);

  Rect offset_first = rect;
  offset_first.Offset(vector);
  offset_first.Inset(insets);

  Rect inset_by_offset = rect;
  inset_by_offset.Inset(insets.Offset(vector));

  EXPECT_EQ(inset_first, offset_first);
  EXPECT_EQ(inset_by_offset, inset_first);
}

TEST(InsetsTest, Scale) {
  Insets in(7, 5);

  Insets test = ScaleToFlooredInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets(24, 12), test);
  test = ScaleToFlooredInsets(in, 2.5f);
  EXPECT_EQ(Insets(17, 12), test);

  test = ScaleToCeiledInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets(25, 13), test);
  test = ScaleToCeiledInsets(in, 2.5f);
  EXPECT_EQ(Insets(18, 13), test);

  test = ScaleToRoundedInsets(in, 2.49f, 3.49f);
  EXPECT_EQ(Insets(24, 12), test);
  test = ScaleToRoundedInsets(in, 2.49f);
  EXPECT_EQ(Insets(17, 12), test);

  test = ScaleToRoundedInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets(25, 13), test);
  test = ScaleToRoundedInsets(in, 2.5f);
  EXPECT_EQ(Insets(18, 13), test);
}

TEST(InsetsTest, ScaleNegative) {
  Insets in(-7, -5);

  Insets test = ScaleToFlooredInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets(-25, -13), test);
  test = ScaleToFlooredInsets(in, 2.5f);
  EXPECT_EQ(Insets(-18, -13), test);

  test = ScaleToCeiledInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets(-24, -12), test);
  test = ScaleToCeiledInsets(in, 2.5f);
  EXPECT_EQ(Insets(-17, -12), test);

  test = ScaleToRoundedInsets(in, 2.49f, 3.49f);
  EXPECT_EQ(Insets(-24, -12), test);
  test = ScaleToRoundedInsets(in, 2.49f);
  EXPECT_EQ(Insets(-17, -12), test);

  test = ScaleToRoundedInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets(-25, -13), test);
  test = ScaleToRoundedInsets(in, 2.5f);
  EXPECT_EQ(Insets(-18, -13), test);
}

TEST(InsetsTest, IntegerOverflow) {
  constexpr int int_min = std::numeric_limits<int>::min();
  constexpr int int_max = std::numeric_limits<int>::max();

  Insets width_height_test(int_max);
  EXPECT_EQ(int_max, width_height_test.width());
  EXPECT_EQ(int_max, width_height_test.height());

  Insets plus_test(int_max);
  plus_test += Insets(int_max);
  EXPECT_EQ(Insets(int_max), plus_test);

  Insets negation_test = -Insets(int_min);
  EXPECT_EQ(Insets(int_max), negation_test);

  Insets scale_test(int_max);
  scale_test = ScaleToRoundedInsets(scale_test, 2.f);
  EXPECT_EQ(Insets(int_max), scale_test);
}

TEST(InsetsTest, IntegerUnderflow) {
  constexpr int int_min = std::numeric_limits<int>::min();
  constexpr int int_max = std::numeric_limits<int>::max();

  Insets width_height_test = Insets(int_min);
  EXPECT_EQ(int_min, width_height_test.width());
  EXPECT_EQ(int_min, width_height_test.height());

  Insets minus_test(int_min);
  minus_test -= Insets(int_max);
  EXPECT_EQ(Insets(int_min), minus_test);

  Insets scale_test = Insets(int_min);
  scale_test = ScaleToRoundedInsets(scale_test, 2.f);
  EXPECT_EQ(Insets(int_min), scale_test);
}

TEST(InsetsTest, IntegerOverflowSetVariants) {
  constexpr int int_max = std::numeric_limits<int>::max();

  Insets set_test(20);
  set_test.set_top(int_max);
  EXPECT_EQ(int_max, set_test.top());
  EXPECT_EQ(0, set_test.bottom());

  set_test.set_left(int_max);
  EXPECT_EQ(int_max, set_test.left());
  EXPECT_EQ(0, set_test.right());

  set_test = Insets(30);
  set_test.set_bottom(int_max);
  EXPECT_EQ(int_max - 30, set_test.bottom());
  EXPECT_EQ(30, set_test.top());

  set_test.set_right(int_max);
  EXPECT_EQ(int_max - 30, set_test.right());
  EXPECT_EQ(30, set_test.left());
}

TEST(InsetsTest, IntegerUnderflowSetVariants) {
  constexpr int int_min = std::numeric_limits<int>::min();

  Insets set_test(-20);
  set_test.set_top(int_min);
  EXPECT_EQ(int_min, set_test.top());
  EXPECT_EQ(0, set_test.bottom());

  set_test.set_left(int_min);
  EXPECT_EQ(int_min, set_test.left());
  EXPECT_EQ(0, set_test.right());

  set_test = Insets(-30);
  set_test.set_bottom(int_min);
  EXPECT_EQ(int_min + 30, set_test.bottom());
  EXPECT_EQ(-30, set_test.top());

  set_test.set_right(int_min);
  EXPECT_EQ(int_min + 30, set_test.right());
  EXPECT_EQ(-30, set_test.left());
}

TEST(InsetsTest, IntegerOverflowSet) {
  constexpr int int_max = std::numeric_limits<int>::max();

  Insets set_all_test;
  set_all_test.Set(10, 20, int_max, int_max);
  EXPECT_EQ(Insets(10, 20, int_max - 10, int_max - 20), set_all_test);
}

TEST(InsetsTest, IntegerOverflowOffset) {
  constexpr int int_max = std::numeric_limits<int>::max();

  const Vector2d max_vector(int_max, int_max);
  Insets insets(1, 2, 3, 4);
  Insets offset_test = insets.Offset(max_vector);
  EXPECT_EQ(Insets(int_max, int_max, 3 - int_max, 4 - int_max), offset_test);
}

TEST(InsetsTest, IntegerUnderflowOffset) {
  constexpr int int_min = std::numeric_limits<int>::min();

  const Vector2d min_vector(int_min, int_min);
  Insets insets(-10);
  Insets offset_test = insets.Offset(min_vector);
  EXPECT_EQ(Insets(int_min, int_min, -10 - int_min, -10 - int_min),
            offset_test);
}

TEST(InsetsTest, Size) {
  Insets insets(1, 2, 3, 4);
  EXPECT_EQ(Size(6, 4), insets.size());
}

TEST(InsetsTest, SetToMax) {
  Insets insets;
  insets.SetToMax(Insets(-1, 2, -3, 4));
  EXPECT_EQ(Insets(0, 2, 0, 4), insets);
  insets.SetToMax(Insets());
  EXPECT_EQ(Insets(0, 2, 0, 4), insets);
  insets.SetToMax(Insets(1, 0, 3, 0));
  EXPECT_EQ(Insets(1, 2, 3, 4), insets);
  insets.SetToMax(Insets(20, 30, 40, 50));
  EXPECT_EQ(Insets(20, 30, 40, 50), insets);

  Insets insets1(-1, -2, -3, -4);
  insets1.SetToMax(Insets());
  EXPECT_EQ(Insets(), insets1);
}

}  // namespace gfx
