// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/rect_conversions.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

constexpr int kMaxInt = std::numeric_limits<int>::max();
constexpr int kMinInt = std::numeric_limits<int>::min();
constexpr float kMaxFloat = std::numeric_limits<float>::max();
constexpr float kEpsilonFloat = std::numeric_limits<float>::epsilon();
constexpr float kMaxIntF = static_cast<float>(kMaxInt);
constexpr float kMinIntF = static_cast<float>(kMinInt);

TEST(RectConversionsTest, ToEnclosedRect) {
  EXPECT_EQ(Rect(), ToEnclosedRect(RectF()));
  EXPECT_EQ(Rect(-1, -1, 2, 2),
            ToEnclosedRect(RectF(-1.5f, -1.5f, 3.0f, 3.0f)));
  EXPECT_EQ(Rect(-1, -1, 3, 3),
            ToEnclosedRect(RectF(-1.5f, -1.5f, 3.5f, 3.5f)));
  EXPECT_EQ(Rect(kMaxInt, kMaxInt, 0, 0),
            ToEnclosedRect(RectF(kMaxFloat, kMaxFloat, 2.0f, 2.0f)));
  EXPECT_EQ(Rect(0, 0, kMaxInt, kMaxInt),
            ToEnclosedRect(RectF(0.0f, 0.0f, kMaxFloat, kMaxFloat)));
  EXPECT_EQ(Rect(20001, 20001, 0, 0),
            ToEnclosedRect(RectF(20000.5f, 20000.5f, 0.5f, 0.5f)));
  EXPECT_EQ(Rect(kMaxInt, kMaxInt, 0, 0),
            ToEnclosedRect(RectF(kMaxIntF, kMaxIntF, kMaxIntF, kMaxIntF)));
  EXPECT_EQ(Rect(2, 3, 5, 5),
            ToEnclosedRect(RectF(1.9999f, 2.0002f, 5.9998f, 6.0001f)));
  EXPECT_EQ(Rect(2, 3, 6, 4),
            ToEnclosedRect(RectF(1.9999f, 2.0001f, 6.0002f, 5.9998f)));
  EXPECT_EQ(Rect(2, 3, 5, 5),
            ToEnclosedRect(RectF(1.9998f, 2.0002f, 6.0001f, 5.9999f)));
}

TEST(RectConversionsTest, ToEnclosedRectHugeRectF) {
  RectF source(kMinIntF, kMinIntF, kMaxIntF * 3.f, kMaxIntF * 3.f);
  Rect enclosed = ToEnclosedRect(source);

  // That rect can't be represented, but it should be big.
  EXPECT_EQ(kMaxInt, enclosed.width());
  EXPECT_EQ(kMaxInt, enclosed.height());
  // It should include some axis near the global origin.
  EXPECT_GT(1, enclosed.x());
  EXPECT_GT(1, enclosed.y());
  // And it should not cause computation issues for itself.
  EXPECT_LT(0, enclosed.right());
  EXPECT_LT(0, enclosed.bottom());
}

TEST(RectConversionsTest, ToEnclosingRect) {
  EXPECT_EQ(Rect(), ToEnclosingRect(RectF()));
  EXPECT_EQ(Rect(5, 5, 0, 0), ToEnclosingRect(RectF(5.5f, 5.5f, 0.0f, 0.0f)));
  EXPECT_EQ(Rect(3, 2, 0, 0),
            ToEnclosingRect(RectF(3.5f, 2.5f, kEpsilonFloat, -0.0f)));
  EXPECT_EQ(Rect(3, 2, 0, 1), ToEnclosingRect(RectF(3.5f, 2.5f, 0.f, 0.001f)));
  EXPECT_EQ(Rect(-2, -2, 4, 4),
            ToEnclosingRect(RectF(-1.5f, -1.5f, 3.0f, 3.0f)));
  EXPECT_EQ(Rect(-2, -2, 4, 4),
            ToEnclosingRect(RectF(-1.5f, -1.5f, 3.5f, 3.5f)));
  EXPECT_EQ(Rect(kMaxInt, kMaxInt, 0, 0),
            ToEnclosingRect(RectF(kMaxFloat, kMaxFloat, 2.0f, 2.0f)));
  EXPECT_EQ(Rect(0, 0, kMaxInt, kMaxInt),
            ToEnclosingRect(RectF(0.0f, 0.0f, kMaxFloat, kMaxFloat)));
  EXPECT_EQ(Rect(20000, 20000, 1, 1),
            ToEnclosingRect(RectF(20000.5f, 20000.5f, 0.5f, 0.5f)));
  EXPECT_EQ(Rect(kMaxInt, kMaxInt, 0, 0),
            ToEnclosingRect(RectF(kMaxIntF, kMaxIntF, kMaxIntF, kMaxIntF)));
  EXPECT_EQ(Rect(-1, -1, 22777713, 2),
            ToEnclosingRect(RectF(-0.5f, -0.5f, 22777712.f, 1.f)));
  EXPECT_EQ(Rect(1, 2, 7, 7),
            ToEnclosingRect(RectF(1.9999f, 2.0002f, 5.9998f, 6.0001f)));
  EXPECT_EQ(Rect(1, 2, 8, 6),
            ToEnclosingRect(RectF(1.9999f, 2.0001f, 6.0002f, 5.9998f)));
  EXPECT_EQ(Rect(1, 2, 7, 7),
            ToEnclosingRect(RectF(1.9998f, 2.0002f, 6.0001f, 5.9999f)));
}

TEST(RectConversionsTest, ToEnclosingRectHugeRectF) {
  RectF source(kMinIntF, kMinIntF, kMaxIntF * 3.f, kMaxIntF * 3.f);
  Rect enclosing = ToEnclosingRect(source);

  // That rect can't be represented, but it should be big.
  EXPECT_EQ(kMaxInt, enclosing.width());
  EXPECT_EQ(kMaxInt, enclosing.height());
  // It should include some axis near the global origin.
  EXPECT_GT(1, enclosing.x());
  EXPECT_GT(1, enclosing.y());
  // And it should cause computation issues for itself.
  EXPECT_LT(0, enclosing.right());
  EXPECT_LT(0, enclosing.bottom());
}

TEST(RectConversionsTest, ToEnclosingRectIgnoringError) {
  static constexpr float kError = 0.001f;
  EXPECT_EQ(Rect(), ToEnclosingRectIgnoringError(RectF(), kError));
  EXPECT_EQ(Rect(5, 5, 0, 0), ToEnclosingRectIgnoringError(
                                  RectF(5.5f, 5.5f, 0.0f, 0.0f), kError));
  EXPECT_EQ(Rect(3, 2, 0, 0),
            ToEnclosingRectIgnoringError(
                RectF(3.5f, 2.5f, kEpsilonFloat, -0.0f), kError));
  EXPECT_EQ(Rect(3, 2, 0, 1), ToEnclosingRectIgnoringError(
                                  RectF(3.5f, 2.5f, 0.f, 0.001f), kError));
  EXPECT_EQ(Rect(-2, -2, 4, 4), ToEnclosingRectIgnoringError(
                                    RectF(-1.5f, -1.5f, 3.0f, 3.0f), kError));
  EXPECT_EQ(Rect(-2, -2, 4, 4), ToEnclosingRectIgnoringError(
                                    RectF(-1.5f, -1.5f, 3.5f, 3.5f), kError));
  EXPECT_EQ(Rect(kMaxInt, kMaxInt, 0, 0),
            ToEnclosingRectIgnoringError(
                RectF(kMaxFloat, kMaxFloat, 2.0f, 2.0f), kError));
  EXPECT_EQ(Rect(0, 0, kMaxInt, kMaxInt),
            ToEnclosingRectIgnoringError(
                RectF(0.0f, 0.0f, kMaxFloat, kMaxFloat), kError));
  EXPECT_EQ(Rect(20000, 20000, 1, 1),
            ToEnclosingRectIgnoringError(RectF(20000.5f, 20000.5f, 0.5f, 0.5f),
                                         kError));
  EXPECT_EQ(Rect(kMaxInt, kMaxInt, 0, 0),
            ToEnclosingRectIgnoringError(
                RectF(kMaxIntF, kMaxIntF, kMaxIntF, kMaxIntF), kError));
  EXPECT_EQ(Rect(-1, -1, 22777713, 2),
            ToEnclosingRectIgnoringError(RectF(-0.5f, -0.5f, 22777712.f, 1.f),
                                         kError));
  EXPECT_EQ(Rect(2, 2, 6, 6),
            ToEnclosingRectIgnoringError(
                RectF(1.9999f, 2.0002f, 5.9998f, 6.0001f), kError));
  EXPECT_EQ(Rect(2, 2, 6, 6),
            ToEnclosingRectIgnoringError(
                RectF(1.9999f, 2.0001f, 6.0002f, 5.9998f), kError));
  EXPECT_EQ(Rect(2, 2, 6, 6),
            ToEnclosingRectIgnoringError(
                RectF(1.9998f, 2.0002f, 6.0001f, 5.9999f), kError));
}

TEST(RectConversionsTest, ToNearestRect) {
  Rect rect;
  EXPECT_EQ(rect, ToNearestRect(RectF(rect)));

  rect = Rect(-1, -1, 3, 3);
  EXPECT_EQ(rect, ToNearestRect(RectF(rect)));

  RectF rectf(-1.00001f, -0.999999f, 3.0000001f, 2.999999f);
  EXPECT_EQ(rect, ToNearestRect(rectf));
}

TEST(RectConversionsTest, ToFlooredRect) {
  EXPECT_EQ(Rect(), ToFlooredRectDeprecated(RectF()));
  EXPECT_EQ(Rect(-2, -2, 3, 3),
            ToFlooredRectDeprecated(RectF(-1.5f, -1.5f, 3.0f, 3.0f)));
  EXPECT_EQ(Rect(-2, -2, 3, 3),
            ToFlooredRectDeprecated(RectF(-1.5f, -1.5f, 3.5f, 3.5f)));
  EXPECT_EQ(Rect(20000, 20000, 0, 0),
            ToFlooredRectDeprecated(RectF(20000.5f, 20000.5f, 0.5f, 0.5f)));
}

}  // namespace gfx
