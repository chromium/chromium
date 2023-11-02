// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/size_f.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/test/geometry_util.h"

namespace gfx {

TEST(SizeFTest, SizeToSizeF) {
  // Check that explicit conversion from integer to float compiles.
  Size a(10, 20);
  EXPECT_EQ(10, SizeF(a).width());
  EXPECT_EQ(20, SizeF(a).height());

  SizeF b(10, 20);
  EXPECT_EQ(b, gfx::SizeF(a));
}

TEST(SizeFTest, ToFlooredSize) {
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0, 0)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.0001f, 0.0001f)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.4999f, 0.4999f)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.5f, 0.5f)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.9999f, 0.9999f)));

  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10, 10)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.0001f, 10.0001f)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.4999f, 10.4999f)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.5f, 10.5f)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.9999f, 10.9999f)));
}

TEST(SizeFTest, ToCeiledSize) {
  EXPECT_EQ(Size(0, 0), ToCeiledSize(SizeF(0, 0)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.0001f, 0.0001f)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.4999f, 0.4999f)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.5f, 0.5f)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.9999f, 0.9999f)));

  EXPECT_EQ(Size(10, 10), ToCeiledSize(SizeF(10, 10)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.0001f, 10.0001f)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.4999f, 10.4999f)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.5f, 10.5f)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.9999f, 10.9999f)));
}

TEST(SizeFTest, ToRoundedSize) {
  EXPECT_EQ(Size(0, 0), ToRoundedSize(SizeF(0, 0)));
  EXPECT_EQ(Size(0, 0), ToRoundedSize(SizeF(0.0001f, 0.0001f)));
  EXPECT_EQ(Size(0, 0), ToRoundedSize(SizeF(0.4999f, 0.4999f)));
  EXPECT_EQ(Size(1, 1), ToRoundedSize(SizeF(0.5f, 0.5f)));
  EXPECT_EQ(Size(1, 1), ToRoundedSize(SizeF(0.9999f, 0.9999f)));

  EXPECT_EQ(Size(10, 10), ToRoundedSize(SizeF(10, 10)));
  EXPECT_EQ(Size(10, 10), ToRoundedSize(SizeF(10.0001f, 10.0001f)));
  EXPECT_EQ(Size(10, 10), ToRoundedSize(SizeF(10.4999f, 10.4999f)));
  EXPECT_EQ(Size(11, 11), ToRoundedSize(SizeF(10.5f, 10.5f)));
  EXPECT_EQ(Size(11, 11), ToRoundedSize(SizeF(10.9999f, 10.9999f)));
}

TEST(SizeFTest, SetToMinMax) {
  SizeF a;

  a = SizeF(3.5f, 5.5f);
  EXPECT_EQ(SizeF(3.5f, 5.5f).ToString(), a.ToString());
  a.SetToMax(SizeF(2.5f, 4.5f));
  EXPECT_EQ(SizeF(3.5f, 5.5f).ToString(), a.ToString());
  a.SetToMax(SizeF(3.5f, 5.5f));
  EXPECT_EQ(SizeF(3.5f, 5.5f).ToString(), a.ToString());
  a.SetToMax(SizeF(4.5f, 2.5f));
  EXPECT_EQ(SizeF(4.5f, 5.5f).ToString(), a.ToString());
  a.SetToMax(SizeF(8.5f, 10.5f));
  EXPECT_EQ(SizeF(8.5f, 10.5f).ToString(), a.ToString());

  a.SetToMin(SizeF(9.5f, 11.5f));
  EXPECT_EQ(SizeF(8.5f, 10.5f).ToString(), a.ToString());
  a.SetToMin(SizeF(8.5f, 10.5f));
  EXPECT_EQ(SizeF(8.5f, 10.5f).ToString(), a.ToString());
  a.SetToMin(SizeF(11.5f, 9.5f));
  EXPECT_EQ(SizeF(8.5f, 9.5f).ToString(), a.ToString());
  a.SetToMin(SizeF(7.5f, 11.5f));
  EXPECT_EQ(SizeF(7.5f, 9.5f).ToString(), a.ToString());
  a.SetToMin(SizeF(3.5f, 5.5f));
  EXPECT_EQ(SizeF(3.5f, 5.5f).ToString(), a.ToString());
}

TEST(SizeFTest, OperatorAddSub) {
  SizeF lhs(100.5f, 20);
  SizeF rhs(50, 10.25f);

  lhs += rhs;
  EXPECT_EQ(SizeF(150.5f, 30.25f), lhs);

  lhs = SizeF(100, 20.25f);
  EXPECT_EQ(SizeF(150, 30.5f), lhs + rhs);

  lhs = SizeF(100.5f, 20);
  lhs -= rhs;
  EXPECT_EQ(SizeF(50.5f, 9.75f), lhs);

  lhs = SizeF(100, 20.75f);
  EXPECT_EQ(SizeF(50, 10.5f), lhs - rhs);

  EXPECT_EQ(SizeF(0, 0), rhs - lhs);
  rhs -= lhs;
  EXPECT_EQ(SizeF(0, 0), rhs);
}

TEST(SizeFTest, IsEmpty) {
  const float clearly_trivial = SizeF::kTrivial / 2.f;
  const float massize_dimension = 4e13f;

  // First, using the constructor.
  EXPECT_TRUE(SizeF(clearly_trivial, 1.f).IsEmpty());
  EXPECT_TRUE(SizeF(.01f, clearly_trivial).IsEmpty());
  EXPECT_TRUE(SizeF(0.f, 0.f).IsEmpty());
  EXPECT_FALSE(SizeF(.01f, .01f).IsEmpty());

  // Then use the setter.
  SizeF test(2.f, 1.f);
  EXPECT_FALSE(test.IsEmpty());

  test.SetSize(clearly_trivial, 1.f);
  EXPECT_TRUE(test.IsEmpty());

  test.SetSize(.01f, clearly_trivial);
  EXPECT_TRUE(test.IsEmpty());

  test.SetSize(0.f, 0.f);
  EXPECT_TRUE(test.IsEmpty());

  test.SetSize(.01f, .01f);
  EXPECT_FALSE(test.IsEmpty());

  // Now just one dimension at a time.
  test.set_width(clearly_trivial);
  EXPECT_TRUE(test.IsEmpty());

  test.set_width(massize_dimension);
  test.set_height(clearly_trivial);
  EXPECT_TRUE(test.IsEmpty());

  test.set_width(clearly_trivial);
  test.set_height(massize_dimension);
  EXPECT_TRUE(test.IsEmpty());

  test.set_width(2.f);
  EXPECT_FALSE(test.IsEmpty());
}

// These are the ramifications of the decision to keep the recorded size
// at zero for trivial sizes.
TEST(SizeFTest, ClampsToZero) {
  const float clearly_trivial = SizeF::kTrivial / 2.f;
  const float nearly_trivial = SizeF::kTrivial * 1.5f;

  SizeF test(clearly_trivial, 1.f);

  EXPECT_FLOAT_EQ(0.f, test.width());
  EXPECT_FLOAT_EQ(1.f, test.height());

  test.SetSize(.01f, clearly_trivial);

  EXPECT_FLOAT_EQ(.01f, test.width());
  EXPECT_FLOAT_EQ(0.f, test.height());

  test.SetSize(nearly_trivial, nearly_trivial);

  EXPECT_FLOAT_EQ(nearly_trivial, test.width());
  EXPECT_FLOAT_EQ(nearly_trivial, test.height());

  test.Scale(0.5f);

  EXPECT_FLOAT_EQ(0.f, test.width());
  EXPECT_FLOAT_EQ(0.f, test.height());

  test.SetSize(0.f, 0.f);
  test.Enlarge(clearly_trivial, clearly_trivial);
  test.Enlarge(clearly_trivial, clearly_trivial);
  test.Enlarge(clearly_trivial, clearly_trivial);

  EXPECT_EQ(SizeF(0.f, 0.f), test);
}

// These make sure the constructor and setter have the same effect on the
// boundary case. This claims to know the boundary, but not which way it goes.
TEST(SizeFTest, ConsistentClamping) {
  SizeF resized;

  resized.SetSize(SizeF::kTrivial, 0.f);
  EXPECT_EQ(SizeF(SizeF::kTrivial, 0.f), resized);

  resized.SetSize(0.f, SizeF::kTrivial);
  EXPECT_EQ(SizeF(0.f, SizeF::kTrivial), resized);
}

TEST(SizeFTest, Transpose) {
  gfx::SizeF s(1.5f, 2.5f);
  EXPECT_EQ(gfx::SizeF(2.5f, 1.5f), TransposeSize(s));
  s.Transpose();
  EXPECT_EQ(gfx::SizeF(2.5f, 1.5f), s);
}

TEST(SizeFTest, ToString) {
  EXPECT_EQ("1x2", SizeF(1, 2).ToString());
  EXPECT_EQ("1.03125x2.5", SizeF(1.03125, 2.5).ToString());
}

}  // namespace gfx
