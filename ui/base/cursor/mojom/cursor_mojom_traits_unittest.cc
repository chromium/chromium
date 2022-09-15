// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/mojom/cursor_mojom_traits.h"

#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor.mojom.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/skia_util.h"

namespace ui {

namespace {

bool EchoCursor(const ui::Cursor& in, ui::Cursor* out) {
  return mojom::Cursor::DeserializeFromMessage(
      mojom::Cursor::SerializeAsMessage(&in), out);
}

using CursorStructTraitsTest = testing::Test;

}  // namespace

// Test that basic cursor structs are passed correctly across the wire.
TEST_F(CursorStructTraitsTest, TestBuiltIn) {
  for (int i = 0; i < static_cast<int>(ui::mojom::CursorType::kCustom); ++i) {
    ui::mojom::CursorType type = static_cast<ui::mojom::CursorType>(i);
    ui::Cursor input(type);
    input.set_image_scale_factor(1);

    ui::Cursor output;
    ASSERT_TRUE(EchoCursor(input, &output));
    EXPECT_EQ(type, output.type());
  }
}

// Test that cursor bitmaps and metadata are passed correctly across the wire.
TEST_F(CursorStructTraitsTest, TestBitmapCursor) {
  ui::Cursor input(ui::mojom::CursorType::kCustom);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorRED);

  const gfx::Point kHotspot = gfx::Point(5, 2);
  input.set_custom_hotspot(kHotspot);
  input.set_custom_bitmap(bitmap);

  const float kScale = 2.0f;
  input.set_image_scale_factor(kScale);

  ui::Cursor output;
  EXPECT_TRUE(EchoCursor(input, &output));
  EXPECT_EQ(input, output);

  EXPECT_EQ(ui::mojom::CursorType::kCustom, output.type());
  EXPECT_EQ(kScale, output.image_scale_factor());
  EXPECT_EQ(kHotspot, output.custom_hotspot());

  // Even though the pixel data is the same, the bitmap generation ids differ.
  EXPECT_TRUE(
      gfx::BitmapsAreEqual(input.custom_bitmap(), output.custom_bitmap()));
  EXPECT_NE(input.custom_bitmap().getGenerationID(),
            output.custom_bitmap().getGenerationID());

  // Make a copy of output; the bitmap generation ids should be the same.
  ui::Cursor copy = output;
  EXPECT_EQ(output.custom_bitmap().getGenerationID(),
            copy.custom_bitmap().getGenerationID());
  EXPECT_EQ(input, output);
}

// Test that empty bitmaps are passed correctly over the wire. This happens when
// renderers relay a custom cursor before the bitmap resource is loaded.
TEST_F(CursorStructTraitsTest, TestEmptyCursor) {
  const gfx::Point kHotspot = gfx::Point(5, 2);
  const float kScale = 2.0f;

  ui::Cursor input(ui::mojom::CursorType::kCustom);
  input.set_custom_hotspot(kHotspot);
  input.set_custom_bitmap(SkBitmap());
  input.set_image_scale_factor(kScale);

  ui::Cursor output;
  ASSERT_TRUE(EchoCursor(input, &output));

  EXPECT_TRUE(output.custom_bitmap().empty());
}

// Test that various device scale factors are passed correctly over the wire.
TEST_F(CursorStructTraitsTest, TestDeviceScaleFactors) {
  ui::Cursor input(ui::mojom::CursorType::kCustom);
  ui::Cursor output;

  for (auto scale : {0.f, 0.525f, 0.75f, 0.9f, 1.f, 2.1f, 2.5f, 3.f, 10.f}) {
    SCOPED_TRACE(testing::Message() << " scale: " << scale);
    input.set_image_scale_factor(scale);
    EXPECT_TRUE(EchoCursor(input, &output));
    EXPECT_EQ(scale, output.image_scale_factor());
  }
}

}  // namespace ui
