// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/normalized_geometry.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace views {

TEST(NormalizedRectTest, Inset_NormalizedInsets) {
  NormalizedRect rect(1, 2, 10, 11);
  constexpr NormalizedInsets kInsets(1, 2, 3, 4);
  rect.Inset(kInsets);
  EXPECT_EQ(2, rect.origin_main());
  EXPECT_EQ(4, rect.origin_cross());
  EXPECT_EQ(6, rect.size_main());
  EXPECT_EQ(5, rect.size_cross());
}

TEST(NormalizedRectTest, Inset_FourValue) {
  NormalizedRect rect(1, 2, 10, 11);
  rect.Inset(1, 2, 3, 4);
  EXPECT_EQ(2, rect.origin_main());
  EXPECT_EQ(4, rect.origin_cross());
  EXPECT_EQ(6, rect.size_main());
  EXPECT_EQ(5, rect.size_cross());
}

TEST(NormalizedRectTest, Inset_TwoValue) {
  NormalizedRect rect(1, 2, 10, 11);
  rect.Inset(3, 4);
  EXPECT_EQ(4, rect.origin_main());
  EXPECT_EQ(6, rect.origin_cross());
  EXPECT_EQ(4, rect.size_main());
  EXPECT_EQ(3, rect.size_cross());
}

TEST(NormalizedRectTest, Inset_Negative) {
  NormalizedRect rect(1, 2, 10, 11);
  rect.Inset(-1, -2, -3, -4);
  EXPECT_EQ(0, rect.origin_main());
  EXPECT_EQ(0, rect.origin_cross());
  EXPECT_EQ(14, rect.size_main());
  EXPECT_EQ(17, rect.size_cross());
}

TEST(NormalizedGeometryTest, GetMainAxis_Size) {
  gfx::Size size(1, 2);

  EXPECT_EQ(1, GetMainAxis(LayoutOrientation::kHorizontal, size));
  EXPECT_EQ(2, GetMainAxis(LayoutOrientation::kVertical, size));
}

TEST(NormalizedGeometryTest, GetMainAxis_SizeBounds) {
  SizeBounds size(1, 2);
  SizeBounds size2;

  EXPECT_EQ(1, GetMainAxis(LayoutOrientation::kHorizontal, size));
  EXPECT_EQ(2, GetMainAxis(LayoutOrientation::kVertical, size));
  EXPECT_EQ(SizeBound(), GetMainAxis(LayoutOrientation::kHorizontal, size2));
  EXPECT_EQ(SizeBound(), GetMainAxis(LayoutOrientation::kVertical, size2));
}

TEST(NormalizedGeometryTest, GetCrossAxis_Size) {
  gfx::Size size(1, 2);

  EXPECT_EQ(2, GetCrossAxis(LayoutOrientation::kHorizontal, size));
  EXPECT_EQ(1, GetCrossAxis(LayoutOrientation::kVertical, size));
}

TEST(NormalizedGeometryTest, GetCrossAxis_SizeBounds) {
  SizeBounds size{1, 2};
  SizeBounds size2;

  EXPECT_EQ(2, GetCrossAxis(LayoutOrientation::kHorizontal, size));
  EXPECT_EQ(1, GetCrossAxis(LayoutOrientation::kVertical, size));
  EXPECT_EQ(SizeBound(), GetCrossAxis(LayoutOrientation::kHorizontal, size2));
  EXPECT_EQ(SizeBound(), GetCrossAxis(LayoutOrientation::kVertical, size2));
}

TEST(NormalizedGeometryTest, SetMainAxis_Size) {
  gfx::Size size(1, 2);

  SetMainAxis(&size, LayoutOrientation::kHorizontal, 3);
  SetMainAxis(&size, LayoutOrientation::kVertical, 4);
  EXPECT_EQ(gfx::Size(3, 4), size);
}

TEST(NormalizedGeometryTest, SetMainAxis_SizeBounds) {
  SizeBounds size(1, 2);

  SetMainAxis(&size, LayoutOrientation::kHorizontal, 3);
  SetMainAxis(&size, LayoutOrientation::kVertical, 4);
  EXPECT_EQ(SizeBounds(3, 4), size);

  SetMainAxis(&size, LayoutOrientation::kHorizontal, SizeBound());
  EXPECT_EQ(SizeBounds(SizeBound(), 4), size);

  SetMainAxis(&size, LayoutOrientation::kVertical, SizeBound());
  EXPECT_EQ(SizeBounds(), size);
}

TEST(NormalizedGeometryTest, SetCrossAxis_Size) {
  gfx::Size size(1, 2);

  SetCrossAxis(&size, LayoutOrientation::kHorizontal, 3);
  SetCrossAxis(&size, LayoutOrientation::kVertical, 4);
  EXPECT_EQ(gfx::Size(4, 3), size);
}

TEST(NormalizedGeometryTest, SetCrossAxis_SizeBounds) {
  SizeBounds size(1, 2);

  SetCrossAxis(&size, LayoutOrientation::kHorizontal, 3);
  SetCrossAxis(&size, LayoutOrientation::kVertical, 4);
  EXPECT_EQ(SizeBounds(4, 3), size);

  SetCrossAxis(&size, LayoutOrientation::kHorizontal, SizeBound());
  EXPECT_EQ(SizeBounds(4, SizeBound()), size);

  SetCrossAxis(&size, LayoutOrientation::kVertical, SizeBound());
  EXPECT_EQ(SizeBounds(), size);
}

}  // namespace views
