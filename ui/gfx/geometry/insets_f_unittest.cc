// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/insets_f.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"

namespace gfx {

TEST(InsetsFTest, Default) {
  InsetsF insets;
  EXPECT_EQ(0, insets.top());
  EXPECT_EQ(0, insets.left());
  EXPECT_EQ(0, insets.bottom());
  EXPECT_EQ(0, insets.right());
}

TEST(InsetsFTest, TLBR) {
  InsetsF insets = InsetsF::TLBR(1.25f, 2.5f, 3.75f, 4.875f);
  EXPECT_EQ(1.25f, insets.top());
  EXPECT_EQ(2.5f, insets.left());
  EXPECT_EQ(3.75f, insets.bottom());
  EXPECT_EQ(4.875f, insets.right());
}

TEST(InsetsFTest, VH) {
  InsetsF insets = InsetsF::VH(1.25f, 2.5f);
  EXPECT_EQ(1.25f, insets.top());
  EXPECT_EQ(2.5f, insets.left());
  EXPECT_EQ(1.25f, insets.bottom());
  EXPECT_EQ(2.5f, insets.right());
}

TEST(InsetsFTest, SetTop) {
  InsetsF insets = InsetsF(1.5f);
  insets.set_top(2.75f);
  EXPECT_EQ(2.75f, insets.top());
  EXPECT_EQ(1.5f, insets.left());
  EXPECT_EQ(1.5f, insets.bottom());
  EXPECT_EQ(1.5f, insets.right());
  EXPECT_EQ(insets, InsetsF(1.5f).set_top(2.75f));
}

TEST(InsetsFTest, SetBottom) {
  InsetsF insets(1.5f);
  insets.set_bottom(2.75f);
  EXPECT_EQ(1.5f, insets.top());
  EXPECT_EQ(1.5f, insets.left());
  EXPECT_EQ(2.75f, insets.bottom());
  EXPECT_EQ(1.5f, insets.right());
  EXPECT_EQ(insets, InsetsF(1.5f).set_bottom(2.75f));
}

TEST(InsetsFTest, SetLeft) {
  InsetsF insets(1.5f);
  insets.set_left(2.75f);
  EXPECT_EQ(1.5f, insets.top());
  EXPECT_EQ(2.75f, insets.left());
  EXPECT_EQ(1.5f, insets.bottom());
  EXPECT_EQ(1.5f, insets.right());
  EXPECT_EQ(insets, InsetsF(1.5f).set_left(2.75f));
}

TEST(InsetsFTest, SetRight) {
  InsetsF insets(1.5f);
  insets.set_right(2.75f);
  EXPECT_EQ(1.5f, insets.top());
  EXPECT_EQ(1.5f, insets.left());
  EXPECT_EQ(1.5f, insets.bottom());
  EXPECT_EQ(2.75f, insets.right());
  EXPECT_EQ(insets, InsetsF(1.5f).set_right(2.75f));
}

TEST(InsetsFTest, WidthHeightAndIsEmpty) {
  InsetsF insets;
  EXPECT_EQ(0, insets.width());
  EXPECT_EQ(0, insets.height());
  EXPECT_TRUE(insets.IsEmpty());

  insets.set_left(3.5f).set_right(4.25f);
  EXPECT_EQ(7.75f, insets.width());
  EXPECT_EQ(0, insets.height());
  EXPECT_FALSE(insets.IsEmpty());

  insets.set_left(0).set_right(0).set_top(1.5f).set_bottom(2.75f);
  EXPECT_EQ(0, insets.width());
  EXPECT_EQ(4.25f, insets.height());
  EXPECT_FALSE(insets.IsEmpty());

  insets.set_left(4.25f).set_right(5);
  EXPECT_EQ(9.25f, insets.width());
  EXPECT_EQ(4.25f, insets.height());
  EXPECT_FALSE(insets.IsEmpty());
}

TEST(InsetsFTest, Operators) {
  InsetsF insets =
      InsetsF().set_left(2.5f).set_right(4.1f).set_top(1.f).set_bottom(3.3f);
  insets +=
      InsetsF().set_left(6.7f).set_right(8.5f).set_top(5.8f).set_bottom(7.6f);
  EXPECT_FLOAT_EQ(6.8f, insets.top());
  EXPECT_FLOAT_EQ(9.2f, insets.left());
  EXPECT_FLOAT_EQ(10.9f, insets.bottom());
  EXPECT_FLOAT_EQ(12.6f, insets.right());

  insets -=
      InsetsF().set_left(0).set_right(2.2f).set_top(-1.f).set_bottom(1.1f);
  EXPECT_FLOAT_EQ(7.8f, insets.top());
  EXPECT_FLOAT_EQ(9.2f, insets.left());
  EXPECT_FLOAT_EQ(9.8f, insets.bottom());
  EXPECT_FLOAT_EQ(10.4f, insets.right());

  insets =
      InsetsF().set_left(10.1f).set_right(10.001f).set_top(10).set_bottom(
          10.01f) +
      InsetsF().set_left(5.f).set_right(-20.2f).set_top(5.5f).set_bottom(0);
  EXPECT_FLOAT_EQ(15.5f, insets.top());
  EXPECT_FLOAT_EQ(15.1f, insets.left());
  EXPECT_FLOAT_EQ(10.01f, insets.bottom());
  EXPECT_FLOAT_EQ(-10.199f, insets.right());

  insets =
      InsetsF().set_left(10.1f).set_right(10.001f).set_top(10).set_bottom(
          10.01f) -
      InsetsF().set_left(5.f).set_right(-20.2f).set_top(5.5f).set_bottom(0);
  EXPECT_FLOAT_EQ(4.5f, insets.top());
  EXPECT_FLOAT_EQ(5.1f, insets.left());
  EXPECT_FLOAT_EQ(10.01f, insets.bottom());
  EXPECT_FLOAT_EQ(30.201f, insets.right());
}

TEST(InsetsFTest, Equality) {
  InsetsF insets1 =
      InsetsF().set_left(2.2f).set_right(4.4f).set_top(1.1f).set_bottom(3.3f);
  InsetsF insets2;
  // Test operator== and operator!=.
  EXPECT_FALSE(insets1 == insets2);
  EXPECT_TRUE(insets1 != insets2);

  insets2.set_left(2.2f).set_right(4.4f).set_top(1.1f).set_bottom(3.3f);
  EXPECT_TRUE(insets1 == insets2);
  EXPECT_FALSE(insets1 != insets2);
}

TEST(InsetsFTest, ToString) {
  InsetsF insets =
      InsetsF().set_left(2.2).set_right(4.4).set_top(1.1).set_bottom(3.3);
  EXPECT_EQ("x:2.2,4.4 y:1.1,3.3", insets.ToString());
}

TEST(InsetsFTest, Scale) {
  InsetsF in = InsetsF().set_left(5).set_right(1).set_top(7).set_bottom(3);
  InsetsF testf = ScaleInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(InsetsF().set_left(12.5f).set_right(2.5f).set_top(24.5f).set_bottom(
                10.5f),
            testf);
  testf = ScaleInsets(in, 2.5f);
  EXPECT_EQ(
      InsetsF().set_left(12.5f).set_right(2.5f).set_top(17.5f).set_bottom(7.5f),
      testf);

  in.Scale(2.5f, 3.5f);
  EXPECT_EQ(InsetsF().set_left(12.5f).set_right(2.5f).set_top(24.5f).set_bottom(
                10.5f),
            in);
  in.Scale(-2.5f);
  EXPECT_EQ(
      InsetsF().set_left(-31.25f).set_right(-6.25f).set_top(-61.25f).set_bottom(
          -26.25f),
      in);
}

TEST(InsetsFTest, ScaleNegative) {
  InsetsF in = InsetsF().set_left(-5).set_right(-1).set_top(-7).set_bottom(-3);

  InsetsF testf = ScaleInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(
      InsetsF().set_left(-12.5f).set_right(-2.5f).set_top(-24.5f).set_bottom(
          -10.5f),
      testf);
  testf = ScaleInsets(in, 2.5f);
  EXPECT_EQ(
      InsetsF().set_left(-12.5f).set_right(-2.5f).set_top(-17.5f).set_bottom(
          -7.5f),
      testf);

  in.Scale(2.5f, 3.5f);
  EXPECT_EQ(
      InsetsF().set_left(-12.5f).set_right(-2.5f).set_top(-24.5f).set_bottom(
          -10.5f),
      in);
  in.Scale(-2.5f);
  EXPECT_EQ(
      InsetsF().set_left(31.25f).set_right(6.25f).set_top(61.25f).set_bottom(
          26.25f),
      in);
}

TEST(InsetsFTest, SetToMax) {
  InsetsF insets;
  insets.SetToMax(
      InsetsF().set_left(2.5f).set_right(4.5f).set_top(-1.25f).set_bottom(
          -2.5f));
  EXPECT_EQ(InsetsF().set_left(2.5f).set_right(4.5f), insets);
  insets.SetToMax(InsetsF());
  EXPECT_EQ(InsetsF().set_left(2.5f).set_right(4.5f), insets);
  insets.SetToMax(InsetsF().set_top(1.25f).set_bottom(3.75f));
  EXPECT_EQ(
      InsetsF().set_left(2.5f).set_right(4.5f).set_top(1.25f).set_bottom(3.75f),
      insets);
  insets.SetToMax(
      InsetsF().set_left(30).set_right(50).set_top(20).set_bottom(40));
  EXPECT_EQ(InsetsF().set_left(30).set_right(50).set_top(20).set_bottom(40),
            insets);

  InsetsF insets1 =
      InsetsF().set_left(-2).set_right(-4).set_top(-1).set_bottom(-3);
  insets1.SetToMax(InsetsF());
  EXPECT_EQ(InsetsF(), insets1);
}

TEST(InsetsFTest, ConversionFromToOutsetsF) {
  InsetsF insets =
      InsetsF().set_left(2.5f).set_right(4.5f).set_top(-1.25f).set_bottom(
          -2.5f);
  EXPECT_EQ(
      OutsetsF().set_left(-2.5f).set_right(-4.5f).set_top(1.25f).set_bottom(
          2.5f),
      insets.ToOutsets());
  EXPECT_EQ(insets, insets.ToOutsets().ToInsets());
}

}  // namespace gfx
