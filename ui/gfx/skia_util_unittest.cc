// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {

TEST(SkiaUtilTest, BitmapsAreEqual) {
  SkBitmap a, b;
  EXPECT_TRUE(gfx::BitmapsAreEqual(a, b));  // Both bitmaps are null.

  a.allocN32Pixels(0, 0);
  EXPECT_FALSE(gfx::BitmapsAreEqual(a, b));  // isNull() differs.
  b.allocN32Pixels(10, 0);
  EXPECT_FALSE(gfx::BitmapsAreEqual(a, b));  // Dimensions differ.
  a.allocN32Pixels(0, 10);
  EXPECT_FALSE(gfx::BitmapsAreEqual(a, b));  // Dimensions still differ.
  b.allocN32Pixels(0, 10);
  EXPECT_TRUE(gfx::BitmapsAreEqual(a, b));  // Dimensions equal (but empty).
  a.allocN32Pixels(10, 10);
  EXPECT_FALSE(gfx::BitmapsAreEqual(a, b));  // Dimensions differ.
  b.allocN32Pixels(10, 10);
  EXPECT_TRUE(gfx::BitmapsAreEqual(a, b));  // Dimensions equal (non-empty).

  a.eraseColor(SK_ColorRED);
  EXPECT_FALSE(gfx::BitmapsAreEqual(a, b));  // Contents differ.
  b.eraseColor(SK_ColorGREEN);
  EXPECT_FALSE(gfx::BitmapsAreEqual(a, b));  // Contents still differ.
  b.eraseColor(SK_ColorRED);
  EXPECT_TRUE(gfx::BitmapsAreEqual(a, b));  // Contents equal.

  a.eraseColor(SK_ColorBLUE);
  EXPECT_FALSE(gfx::BitmapsAreEqual(a, b));  // Contents differ.
  b = a;
  EXPECT_TRUE(gfx::BitmapsAreEqual(a, b));  // Generation ids equal.

  a.reset();
  EXPECT_FALSE(gfx::BitmapsAreEqual(a, b));  // isNull() differs.
  b.reset();
  EXPECT_TRUE(gfx::BitmapsAreEqual(a, b));  // Both bitmaps are null.
}

}  // namespace gfx
