// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/insets.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/outsets.h"
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

TEST(InsetsTest, TLBR) {
  Insets insets = Insets::TLBR(1, 2, 3, 4);
  EXPECT_EQ(1, insets.top());
  EXPECT_EQ(2, insets.left());
  EXPECT_EQ(3, insets.bottom());
  EXPECT_EQ(4, insets.right());
}

TEST(InsetsTest, VH) {
  Insets insets = Insets::VH(1, 2);
  EXPECT_EQ(1, insets.top());
  EXPECT_EQ(2, insets.left());
  EXPECT_EQ(1, insets.bottom());
  EXPECT_EQ(2, insets.right());
}

TEST(InsetsTest, SetLeftRight) {
  Insets insets(1);
  insets.set_left_right(3, 4);
  EXPECT_EQ(1, insets.top());
  EXPECT_EQ(3, insets.left());
  EXPECT_EQ(1, insets.bottom());
  EXPECT_EQ(4, insets.right());

  EXPECT_EQ(insets, Insets(1).set_left_right(3, 4));
}

TEST(InsetsTest, SetTopBottom) {
  Insets insets(1);
  insets.set_top_bottom(3, 4);
  EXPECT_EQ(3, insets.top());
  EXPECT_EQ(1, insets.left());
  EXPECT_EQ(4, insets.bottom());
  EXPECT_EQ(1, insets.right());

  EXPECT_EQ(insets, Insets(1).set_top_bottom(3, 4));
}

TEST(InsetsTest, SetTop) {
  Insets insets(1);
  insets.set_top(2);
  EXPECT_EQ(2, insets.top());
  EXPECT_EQ(1, insets.left());
  EXPECT_EQ(1, insets.bottom());
  EXPECT_EQ(1, insets.right());
  EXPECT_EQ(insets, Insets(1).set_top(2));
}

TEST(InsetsTest, SetBottom) {
  Insets insets(1);
  insets.set_bottom(2);
  EXPECT_EQ(1, insets.top());
  EXPECT_EQ(1, insets.left());
  EXPECT_EQ(2, insets.bottom());
  EXPECT_EQ(1, insets.right());
  EXPECT_EQ(insets, Insets(1).set_bottom(2));
}

TEST(InsetsTest, SetLeft) {
  Insets insets(1);
  insets.set_left(2);
  EXPECT_EQ(1, insets.top());
  EXPECT_EQ(2, insets.left());
  EXPECT_EQ(1, insets.bottom());
  EXPECT_EQ(1, insets.right());
  EXPECT_EQ(insets, Insets(1).set_left(2));
}

TEST(InsetsTest, SetRight) {
  Insets insets(1);
  insets.set_right(2);
  EXPECT_EQ(1, insets.top());
  EXPECT_EQ(1, insets.left());
  EXPECT_EQ(1, insets.bottom());
  EXPECT_EQ(2, insets.right());
  EXPECT_EQ(insets, Insets(1).set_right(2));
}

TEST(InsetsTest, WidthHeightAndIsEmpty) {
  Insets insets;
  EXPECT_EQ(0, insets.width());
  EXPECT_EQ(0, insets.height());
  EXPECT_TRUE(insets.IsEmpty());

  insets.set_left_right(3, 4);
  EXPECT_EQ(7, insets.width());
  EXPECT_EQ(0, insets.height());
  EXPECT_FALSE(insets.IsEmpty());

  insets.set_left_right(0, 0);
  insets.set_top_bottom(1, 2);
  EXPECT_EQ(0, insets.width());
  EXPECT_EQ(3, insets.height());
  EXPECT_FALSE(insets.IsEmpty());

  insets.set_left_right(4, 5);
  EXPECT_EQ(9, insets.width());
  EXPECT_EQ(3, insets.height());
  EXPECT_FALSE(insets.IsEmpty());
}

TEST(InsetsTest, Operators) {
  Insets insets = Insets().set_left_right(2, 4).set_top_bottom(1, 3);
  insets += Insets().set_left_right(6, 8).set_top_bottom(5, 7);
  EXPECT_EQ(6, insets.top());
  EXPECT_EQ(8, insets.left());
  EXPECT_EQ(10, insets.bottom());
  EXPECT_EQ(12, insets.right());

  insets -= Insets().set_left_right(0, 2).set_top_bottom(-1, 1);
  EXPECT_EQ(7, insets.top());
  EXPECT_EQ(8, insets.left());
  EXPECT_EQ(9, insets.bottom());
  EXPECT_EQ(10, insets.right());

  insets = Insets(10) + Insets().set_left_right(5, -20).set_top_bottom(10, 0);
  EXPECT_EQ(20, insets.top());
  EXPECT_EQ(15, insets.left());
  EXPECT_EQ(10, insets.bottom());
  EXPECT_EQ(-10, insets.right());

  insets = Insets(10) - Insets().set_left_right(5, -20).set_top_bottom(10, 0);
  EXPECT_EQ(0, insets.top());
  EXPECT_EQ(5, insets.left());
  EXPECT_EQ(10, insets.bottom());
  EXPECT_EQ(30, insets.right());
}

TEST(InsetsTest, Equality) {
  Insets insets1 = Insets().set_left_right(2, 4).set_top_bottom(1, 3);
  Insets insets2;
  // Test operator== and operator!=.
  EXPECT_FALSE(insets1 == insets2);
  EXPECT_TRUE(insets1 != insets2);

  insets2.set_left_right(2, 4).set_top_bottom(1, 3);
  EXPECT_TRUE(insets1 == insets2);
  EXPECT_FALSE(insets1 != insets2);
}

TEST(InsetsTest, ToString) {
  Insets insets = Insets().set_left_right(2, 4).set_top_bottom(1, 3);
  EXPECT_EQ("x:2,4 y:1,3", insets.ToString());
}

TEST(InsetsTest, Offset) {
  const Insets insets = Insets().set_left_right(2, 4).set_top_bottom(1, 3);
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

  Insets insets_with_offset = insets;
  insets_with_offset.Offset(vector);
  EXPECT_EQ(gfx::Insets().set_left_right(11, -5).set_top_bottom(11, -7),
            insets_with_offset);
  EXPECT_EQ(insets_with_offset, insets + vector);

  Rect inset_by_offset = rect;
  inset_by_offset.Inset(insets_with_offset);

  EXPECT_EQ(inset_first, offset_first);
  EXPECT_EQ(inset_by_offset, inset_first);
}

TEST(InsetsTest, Scale) {
  Insets in = Insets().set_left_right(5, 1).set_top_bottom(7, 3);

  Insets test = ScaleToFlooredInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets().set_left_right(12, 2).set_top_bottom(24, 10), test);
  test = ScaleToFlooredInsets(in, 2.5f);
  EXPECT_EQ(Insets().set_left_right(12, 2).set_top_bottom(17, 7), test);

  test = ScaleToCeiledInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets().set_left_right(13, 3).set_top_bottom(25, 11), test);
  test = ScaleToCeiledInsets(in, 2.5f);
  EXPECT_EQ(Insets().set_left_right(13, 3).set_top_bottom(18, 8), test);

  test = ScaleToRoundedInsets(in, 2.49f, 3.49f);
  EXPECT_EQ(Insets().set_left_right(12, 2).set_top_bottom(24, 10), test);
  test = ScaleToRoundedInsets(in, 2.49f);
  EXPECT_EQ(Insets().set_left_right(12, 2).set_top_bottom(17, 7), test);

  test = ScaleToRoundedInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets().set_left_right(13, 3).set_top_bottom(25, 11), test);
  test = ScaleToRoundedInsets(in, 2.5f);
  EXPECT_EQ(Insets().set_left_right(13, 3).set_top_bottom(18, 8), test);
}

TEST(InsetsTest, ScaleNegative) {
  Insets in = Insets().set_left_right(-5, -1).set_top_bottom(-7, -3);

  Insets test = ScaleToFlooredInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets().set_left_right(-13, -3).set_top_bottom(-25, -11), test);
  test = ScaleToFlooredInsets(in, 2.5f);
  EXPECT_EQ(Insets().set_left_right(-13, -3).set_top_bottom(-18, -8), test);

  test = ScaleToCeiledInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets().set_left_right(-12, -2).set_top_bottom(-24, -10), test);
  test = ScaleToCeiledInsets(in, 2.5f);
  EXPECT_EQ(Insets().set_left_right(-12, -2).set_top_bottom(-17, -7), test);

  test = ScaleToRoundedInsets(in, 2.49f, 3.49f);
  EXPECT_EQ(Insets().set_left_right(-12, -2).set_top_bottom(-24, -10), test);
  test = ScaleToRoundedInsets(in, 2.49f);
  EXPECT_EQ(Insets().set_left_right(-12, -2).set_top_bottom(-17, -7), test);

  test = ScaleToRoundedInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(Insets().set_left_right(-13, -3).set_top_bottom(-25, -11), test);
  test = ScaleToRoundedInsets(in, 2.5f);
  EXPECT_EQ(Insets().set_left_right(-13, -3).set_top_bottom(-18, -8), test);
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

  Insets set_all_test =
      Insets().set_left_right(int_max, 20).set_top_bottom(10, int_max);
  EXPECT_EQ(
      Insets().set_left_right(int_max, 0).set_top_bottom(10, int_max - 10),
      set_all_test);
}

TEST(InsetsTest, IntegerOverflowOffset) {
  constexpr int int_max = std::numeric_limits<int>::max();

  const gfx::Vector2d max_vector(int_max, int_max);
  Insets insets = Insets().set_left_right(2, 4).set_top_bottom(1, 3);
  insets.Offset(max_vector);
  EXPECT_EQ(gfx::Insets()
                .set_left_right(int_max, 4 - int_max)
                .set_top_bottom(int_max, 3 - int_max),
            insets);
}

TEST(InsetsTest, IntegerUnderflowOffset) {
  constexpr int int_min = std::numeric_limits<int>::min();

  const Vector2d min_vector(int_min, int_min);
  Insets insets(-10);
  insets.Offset(min_vector);
  EXPECT_EQ(gfx::Insets()
                .set_left_right(int_min, -10 - int_min)
                .set_top_bottom(int_min, -10 - int_min),
            insets);
}

TEST(InsetsTest, Size) {
  Insets insets = Insets().set_left_right(2, 4).set_top_bottom(1, 3);
  EXPECT_EQ(Size(6, 4), insets.size());
}

TEST(InsetsTest, SetToMax) {
  Insets insets;
  insets.SetToMax(Insets().set_left_right(2, 4).set_top_bottom(-1, -3));
  EXPECT_EQ(Insets().set_left_right(2, 4), insets);
  insets.SetToMax(Insets());
  EXPECT_EQ(Insets().set_left_right(2, 4), insets);
  insets.SetToMax(Insets().set_top_bottom(1, 3));
  EXPECT_EQ(Insets().set_left_right(2, 4).set_top_bottom(1, 3), insets);
  insets.SetToMax(Insets().set_left_right(30, 50).set_top_bottom(20, 40));
  EXPECT_EQ(Insets().set_left_right(30, 50).set_top_bottom(20, 40), insets);

  Insets insets1 = Insets().set_left_right(-2, -4).set_top_bottom(-2, -4);
  insets1.SetToMax(Insets());
  EXPECT_EQ(Insets(), insets1);
}

TEST(InsetsTest, ConversionFromToOutsets) {
  Insets insets = Insets().set_left_right(2, 4).set_top_bottom(-1, -3);
  EXPECT_EQ(Outsets().set_left_right(-2, -4).set_top_bottom(1, 3),
            insets.ToOutsets());
  EXPECT_EQ(insets, insets.ToOutsets().ToInsets());
}

}  // namespace gfx
