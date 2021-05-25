// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <cmath>
#include <limits>

#include "base/cxx17_backports.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace gfx {

TEST(ScrollOffsetTest, IsZero) {
  ScrollOffset zero(0, 0);
  ScrollOffset nonzero(0.1f, -0.1f);

  EXPECT_TRUE(zero.IsZero());
  EXPECT_FALSE(nonzero.IsZero());
}

TEST(ScrollOffsetTest, Add) {
  ScrollOffset f1(3.1f, 5.1f);
  ScrollOffset f2(4.3f, -1.3f);

  const struct {
    ScrollOffset expected;
    ScrollOffset actual;
  } scroll_offset_tests[] = {
    { ScrollOffset(3.1f, 5.1f), f1 + ScrollOffset() },
    { ScrollOffset(3.1f + 4.3f, 5.1f - 1.3f), f1 + f2 },
    { ScrollOffset(3.1f - 4.3f, 5.1f + 1.3f), f1 - f2 }
  };

  for (size_t i = 0; i < base::size(scroll_offset_tests); ++i)
    EXPECT_EQ(scroll_offset_tests[i].expected.ToString(),
              scroll_offset_tests[i].actual.ToString());
}

TEST(ScrollOffsetTest, Negative) {
  const struct {
    ScrollOffset expected;
    ScrollOffset actual;
  } scroll_offset_tests[] = {
    { ScrollOffset(-0.3f, -0.3f), -ScrollOffset(0.3f, 0.3f) },
    { ScrollOffset(0.3f, 0.3f), -ScrollOffset(-0.3f, -0.3f) },
    { ScrollOffset(-0.3f, 0.3f), -ScrollOffset(0.3f, -0.3f) },
    { ScrollOffset(0.3f, -0.3f), -ScrollOffset(-0.3f, 0.3f) }
  };

  for (size_t i = 0; i < base::size(scroll_offset_tests); ++i)
    EXPECT_EQ(scroll_offset_tests[i].expected.ToString(),
              scroll_offset_tests[i].actual.ToString());
}

TEST(ScrollOffsetTest, Scale) {
  float float_values[][4] = {
    { 4.5f, 1.2f, 3.3f, 5.6f },
    { 4.5f, -1.2f, 3.3f, 5.6f },
    { 4.5f, 1.2f, 3.3f, -5.6f },
    { 4.5f, 1.2f, -3.3f, -5.6f },
    { -4.5f, 1.2f, 3.3f, 5.6f },
    { -4.5f, 1.2f, 0, 5.6f },
    { -4.5f, 1.2f, 3.3f, 0 },
    { 4.5f, 0, 3.3f, 5.6f },
    { 0, 1.2f, 3.3f, 5.6f }
  };

  for (size_t i = 0; i < base::size(float_values); ++i) {
    ScrollOffset v(float_values[i][0], float_values[i][1]);
    v.Scale(float_values[i][2], float_values[i][3]);
    EXPECT_EQ(v.x(), float_values[i][0] * float_values[i][2]);
    EXPECT_EQ(v.y(), float_values[i][1] * float_values[i][3]);
  }

  float single_values[][3] = {
    { 4.5f, 1.2f, 3.3f },
    { 4.5f, -1.2f, 3.3f },
    { 4.5f, 1.2f, 3.3f },
    { 4.5f, 1.2f, -3.3f },
    { -4.5f, 1.2f, 3.3f },
    { -4.5f, 1.2f, 0 },
    { -4.5f, 1.2f, 3.3f },
    { 4.5f, 0, 3.3f },
    { 0, 1.2f, 3.3f }
  };

  for (size_t i = 0; i < base::size(single_values); ++i) {
    ScrollOffset v(single_values[i][0], single_values[i][1]);
    v.Scale(single_values[i][2]);
    EXPECT_EQ(v.x(), single_values[i][0] * single_values[i][2]);
    EXPECT_EQ(v.y(), single_values[i][1] * single_values[i][2]);
  }
}

TEST(ScrollOffsetTest, ClampScrollOffset) {
  ScrollOffset a;

  a = ScrollOffset(3.5, 5.5);
  EXPECT_EQ(ScrollOffset(3.5, 5.5).ToString(), a.ToString());
  a.SetToMax(ScrollOffset(2.5, 4.5));
  EXPECT_EQ(ScrollOffset(3.5, 5.5).ToString(), a.ToString());
  a.SetToMax(ScrollOffset(3.5, 5.5));
  EXPECT_EQ(ScrollOffset(3.5, 5.5).ToString(), a.ToString());
  a.SetToMax(ScrollOffset(4.5, 2.5));
  EXPECT_EQ(ScrollOffset(4.5, 5.5).ToString(), a.ToString());
  a.SetToMax(ScrollOffset(8.5, 10.5));
  EXPECT_EQ(ScrollOffset(8.5, 10.5).ToString(), a.ToString());

  a.SetToMin(ScrollOffset(9.5, 11.5));
  EXPECT_EQ(ScrollOffset(8.5, 10.5).ToString(), a.ToString());
  a.SetToMin(ScrollOffset(8.5, 10.5));
  EXPECT_EQ(ScrollOffset(8.5, 10.5).ToString(), a.ToString());
  a.SetToMin(ScrollOffset(11.5, 9.5));
  EXPECT_EQ(ScrollOffset(8.5, 9.5).ToString(), a.ToString());
  a.SetToMin(ScrollOffset(7.5, 11.5));
  EXPECT_EQ(ScrollOffset(7.5, 9.5).ToString(), a.ToString());
  a.SetToMin(ScrollOffset(3.5, 5.5));
  EXPECT_EQ(ScrollOffset(3.5, 5.5).ToString(), a.ToString());
}

}  // namespace gfx
