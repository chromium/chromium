// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/geometry/skia_conversions.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

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

}  // namespace gfx
