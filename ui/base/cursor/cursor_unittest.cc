// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include <algorithm>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_util.h"

namespace ui {
namespace {

TEST(CursorTest, Null) {
  Cursor cursor;
  EXPECT_EQ(CursorType::kNull, cursor.native_type());
}

TEST(CursorTest, BasicType) {
  Cursor cursor(CursorType::kPointer);
  EXPECT_EQ(CursorType::kPointer, cursor.native_type());

  Cursor copy(cursor);
  EXPECT_EQ(cursor, copy);
}

TEST(CursorTest, CustomType) {
  Cursor cursor(CursorType::kCustom);
  EXPECT_EQ(CursorType::kCustom, cursor.native_type());

  const float kScale = 2.0f;
  cursor.set_device_scale_factor(kScale);
  EXPECT_EQ(kScale, cursor.device_scale_factor());

  const gfx::Point kHotspot = gfx::Point(5, 2);
  cursor.set_custom_hotspot(kHotspot);
  EXPECT_EQ(kHotspot, cursor.GetHotspot());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorRED);
  cursor.set_custom_bitmap(bitmap);

  EXPECT_EQ(bitmap.getGenerationID(), cursor.GetBitmap().getGenerationID());
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, cursor.GetBitmap()));

  Cursor copy(cursor);
  EXPECT_EQ(cursor.GetBitmap().getGenerationID(),
            copy.GetBitmap().getGenerationID());
  EXPECT_TRUE(gfx::BitmapsAreEqual(cursor.GetBitmap(), copy.GetBitmap()));
  EXPECT_EQ(cursor, copy);
}

TEST(CursorTest, CustomTypeComparesBitmapPixels) {
  Cursor cursor1(CursorType::kCustom);
  Cursor cursor2(CursorType::kCustom);

  SkBitmap bitmap1;
  bitmap1.allocN32Pixels(10, 10);
  bitmap1.eraseColor(SK_ColorRED);
  cursor1.set_custom_bitmap(bitmap1);

  SkBitmap bitmap2;
  bitmap2.allocN32Pixels(10, 10);
  bitmap2.eraseColor(SK_ColorRED);
  cursor2.set_custom_bitmap(bitmap2);

  EXPECT_NE(cursor1.GetBitmap().getGenerationID(),
            cursor2.GetBitmap().getGenerationID());
  EXPECT_TRUE(gfx::BitmapsAreEqual(cursor1.GetBitmap(), cursor2.GetBitmap()));
  EXPECT_EQ(cursor1, cursor2);
}

}  // namespace
}  // namespace ui
