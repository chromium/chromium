// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/cursor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"

namespace ui {
namespace {

using mojom::CursorType;

TEST(CursorTest, Null) {
  Cursor cursor;
  EXPECT_EQ(CursorType::kNull, cursor.type());
}

TEST(CursorTest, BasicType) {
  Cursor cursor(CursorType::kPointer);
  EXPECT_EQ(CursorType::kPointer, cursor.type());

  Cursor copy(cursor);
  EXPECT_EQ(cursor, copy);
}

TEST(CursorTest, CustomType) {
  const SkBitmap kBitmap = gfx::test::CreateBitmap(/*size=*/10);
  constexpr gfx::Point kHotspot = gfx::Point(5, 2);
  constexpr float kScale = 2.0f;

  const Cursor cursor = ui::Cursor::NewCustom(kBitmap, kHotspot, kScale);
  EXPECT_EQ(CursorType::kCustom, cursor.type());
  EXPECT_EQ(kBitmap.getGenerationID(),
            cursor.custom_bitmap().getGenerationID());
  EXPECT_TRUE(gfx::BitmapsAreEqual(kBitmap, cursor.custom_bitmap()));
  EXPECT_EQ(kHotspot, cursor.custom_hotspot());
  EXPECT_EQ(kScale, cursor.image_scale_factor());

  Cursor copy(cursor);
  EXPECT_EQ(cursor.custom_bitmap().getGenerationID(),
            copy.custom_bitmap().getGenerationID());
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(cursor.custom_bitmap(), copy.custom_bitmap()));
  EXPECT_EQ(cursor, copy);
}

TEST(CursorTest, CustomTypeComparesBitmapPixels) {
  const Cursor kCursor1 =
      Cursor::NewCustom(gfx::test::CreateBitmap(/*size=*/10), gfx::Point());
  const Cursor kCursor2 =
      Cursor::NewCustom(gfx::test::CreateBitmap(/*size=*/10), gfx::Point());

  EXPECT_NE(kCursor1.custom_bitmap().getGenerationID(),
            kCursor2.custom_bitmap().getGenerationID());
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(kCursor1.custom_bitmap(), kCursor2.custom_bitmap()));
  EXPECT_EQ(kCursor1, kCursor2);
}

TEST(CursorTest, ClampHotspot) {
  // Initialize a cursor with an invalid hotspot; it should be clamped.
  const Cursor cursor =
      Cursor::NewCustom(gfx::test::CreateBitmap(5, 7), gfx::Point(100, 100));
  EXPECT_EQ(gfx::Point(4, 6), cursor.custom_hotspot());
}

}  // namespace
}  // namespace ui
