// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/skia_conversions.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace gfx {

TEST(SkiaConversionsTest, SkiaRectConversions) {
  Rect isrc(10, 20, 30, 40);
  RectF fsrc(10.5f, 20.5f, 30.5f, 40.5f);

  SkIRect skirect = RectToSkIRect(isrc);
  EXPECT_EQ(isrc.ToString(), SkIRectToRect(skirect).ToString());

  SkRect skrect = RectToSkRect(isrc);
  EXPECT_EQ(gfx::RectF(isrc).ToString(), SkRectToRectF(skrect).ToString());

  skrect = RectFToSkRect(fsrc);
  EXPECT_EQ(fsrc.ToString(), SkRectToRectF(skrect).ToString());
}

TEST(SkiaConversionsTest, RectToSkRectAccuracy) {
  // For a gfx::Rect with large negative x/y and large with/height, but small
  // right/bottom, we expect the converted SkRect has accurate right/bottom,
  // to make sure the right/bottom edge, which is likely to be visible, to be
  // rendered correctly.
  Rect r;
  for (int i = 0; i < 50; i++) {
    r.SetByBounds(-30000000, -28000000, i, i + 1);
    EXPECT_EQ(i, r.right());
    EXPECT_EQ(i + 1, r.bottom());
    SkRect skrect = RectToSkRect(r);
    EXPECT_EQ(i, skrect.right());
    EXPECT_EQ(i + 1, skrect.bottom());
  }
}

TEST(SkiaConversionsTest, SkIRectToRectClamping) {
  // This clamping only makes sense if SkIRect and gfx::Rect have the same size.
  // Otherwise, either other overflows can occur that we don't handle, or no
  // overflows can ocur.
  if (sizeof(int) != sizeof(int32_t))
    return;
  using Limits = std::numeric_limits<int>;

  // right-left and bottom-top would overflow.
  // These should be mapped to max width/height, which is as close as gfx::Rect
  // can represent.
  Rect result = SkIRectToRect(SkIRect::MakeLTRB(Limits::min(), Limits::min(),
                                                Limits::max(), Limits::max()));
  EXPECT_EQ(gfx::Size(Limits::max(), Limits::max()), result.size());

  // right-left and bottom-top would underflow.
  // These should be mapped to zero, like all negative values.
  result = SkIRectToRect(SkIRect::MakeLTRB(Limits::max(), Limits::max(),
                                           Limits::min(), Limits::min()));
  EXPECT_EQ(gfx::Rect(Limits::max(), Limits::max(), 0, 0), result);
}

TEST(SkiaConversionsTest, TransformSkM44Conversions) {
  std::vector<float> v = {1, 2,  3,  4,  5,  6,  7,  8,
                          9, 10, 11, 12, 13, 14, 15, 16};
  Transform t = Transform::ColMajorF(v.data());

  SkM44 m = TransformToSkM44(t);
  std::vector<float> v1(16);
  m.getColMajor(v1.data());
  EXPECT_EQ(v, v1);
  EXPECT_EQ(t, SkM44ToTransform(m));
}

TEST(SkiaConversionsTest, TransformSkMatrixConversions) {
  std::vector<float> v = {1, 2, 0, 4, 5, 6, 0, 8, 0, 0, 1, 0, 13, 14, 0, 16};
  Transform t = Transform::ColMajorF(v.data());

  std::vector<float> v1(16);
  SkMatrix m = TransformToFlattenedSkMatrix(t);
  SkM44 m44(m);
  m44.getColMajor(v1.data());
  EXPECT_EQ(v, v1);
  EXPECT_EQ(t, SkMatrixToTransform(m));
}

}  // namespace gfx
