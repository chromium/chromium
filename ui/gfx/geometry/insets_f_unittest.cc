// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/insets_f.h"

#include "testing/gtest/include/gtest/gtest.h"
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

TEST(InsetsFTest, InsetsF) {
  InsetsF insets(1.25f, 2.5f, 3.75f, 4.875f);
  EXPECT_EQ(1.25f, insets.top());
  EXPECT_EQ(2.5f, insets.left());
  EXPECT_EQ(3.75f, insets.bottom());
  EXPECT_EQ(4.875f, insets.right());
}

TEST(InsetsFTest, WidthHeightAndIsEmpty) {
  InsetsF insets;
  EXPECT_EQ(0, insets.width());
  EXPECT_EQ(0, insets.height());
  EXPECT_TRUE(insets.IsEmpty());

  insets.Set(0, 3.5f, 0, 4.25f);
  EXPECT_EQ(7.75f, insets.width());
  EXPECT_EQ(0, insets.height());
  EXPECT_FALSE(insets.IsEmpty());

  insets.Set(1.5f, 0, 2.75f, 0);
  EXPECT_EQ(0, insets.width());
  EXPECT_EQ(4.25f, insets.height());
  EXPECT_FALSE(insets.IsEmpty());

  insets.Set(1.5f, 4.25f, 2.75f, 5);
  EXPECT_EQ(9.25f, insets.width());
  EXPECT_EQ(4.25f, insets.height());
  EXPECT_FALSE(insets.IsEmpty());
}

TEST(InsetsFTest, Operators) {
  InsetsF insets(1.f, 2.5f, 3.3f, 4.1f);
  insets += InsetsF(5.8f, 6.7f, 7.6f, 8.5f);
  EXPECT_FLOAT_EQ(6.8f, insets.top());
  EXPECT_FLOAT_EQ(9.2f, insets.left());
  EXPECT_FLOAT_EQ(10.9f, insets.bottom());
  EXPECT_FLOAT_EQ(12.6f, insets.right());

  insets -= InsetsF(-1.f, 0, 1.1f, 2.2f);
  EXPECT_FLOAT_EQ(7.8f, insets.top());
  EXPECT_FLOAT_EQ(9.2f, insets.left());
  EXPECT_FLOAT_EQ(9.8f, insets.bottom());
  EXPECT_FLOAT_EQ(10.4f, insets.right());

  insets = InsetsF(10, 10.1f, 10.01f, 10.001f) + InsetsF(5.5f, 5.f, 0, -20.2f);
  EXPECT_FLOAT_EQ(15.5f, insets.top());
  EXPECT_FLOAT_EQ(15.1f, insets.left());
  EXPECT_FLOAT_EQ(10.01f, insets.bottom());
  EXPECT_FLOAT_EQ(-10.199f, insets.right());

  insets = InsetsF(10, 10.1f, 10.01f, 10.001f) - InsetsF(5.5f, 5.f, 0, -20.2f);
  EXPECT_FLOAT_EQ(4.5f, insets.top());
  EXPECT_FLOAT_EQ(5.1f, insets.left());
  EXPECT_FLOAT_EQ(10.01f, insets.bottom());
  EXPECT_FLOAT_EQ(30.201f, insets.right());
}

TEST(InsetsFTest, Equality) {
  InsetsF insets1(1.1f, 2.2f, 3.3f, 4.4f);
  InsetsF insets2;
  // Test operator== and operator!=.
  EXPECT_FALSE(insets1 == insets2);
  EXPECT_TRUE(insets1 != insets2);

  insets2.Set(1.1f, 2.2f, 3.3f, 4.4f);
  EXPECT_TRUE(insets1 == insets2);
  EXPECT_FALSE(insets1 != insets2);
}

TEST(InsetsFTest, ToString) {
  InsetsF insets(1.1f, 2.2f, 3.3f, 4.4f);
  EXPECT_EQ("1.100000,2.200000,3.300000,4.400000", insets.ToString());
}

TEST(InsetsFTest, Scale) {
  InsetsF in(7, 5, 3, 1);
  InsetsF testf = ScaleInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(InsetsF(24.5f, 12.5f, 10.5f, 2.5f), testf);
  testf = ScaleInsets(in, 2.5f);
  EXPECT_EQ(InsetsF(17.5f, 12.5f, 7.5f, 2.5f), testf);
}

TEST(InsetsFTest, ScaleNegative) {
  InsetsF in = InsetsF(-7, -5, -3, -1);

  InsetsF testf = ScaleInsets(in, 2.5f, 3.5f);
  EXPECT_EQ(InsetsF(-24.5f, -12.5f, -10.5f, -2.5f), testf);
  testf = ScaleInsets(in, 2.5f);
  EXPECT_EQ(InsetsF(-17.5f, -12.5f, -7.5f, -2.5f), testf);
}

TEST(InsetsFTest, SetToMax) {
  InsetsF insets;
  insets.SetToMax(InsetsF(-1.25f, 2.5f, -3.75f, 4.5f));
  EXPECT_EQ(InsetsF(0, 2.5f, 0, 4.5f), insets);
  insets.SetToMax(InsetsF());
  EXPECT_EQ(InsetsF(0, 2.5f, 0, 4.5f), insets);
  insets.SetToMax(InsetsF(1.25f, 0, 3.75f, 0));
  EXPECT_EQ(InsetsF(1.25f, 2.5f, 3.75f, 4.5f), insets);
  insets.SetToMax(InsetsF(20, 30, 40, 50));
  EXPECT_EQ(InsetsF(20, 30, 40, 50), insets);

  InsetsF insets1(-1, -2, -3, -4);
  insets1.SetToMax(InsetsF());
  EXPECT_EQ(InsetsF(), insets1);
}

}  // namespace gfx
