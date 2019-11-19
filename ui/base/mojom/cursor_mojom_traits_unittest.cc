// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mojom/cursor_mojom_traits.h"

#include "mojo/public/cpp/bindings/binding_set.h"
#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/mojom/cursor.mojom.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/skia_util.h"

namespace ui {

namespace {

bool EchoCursor(const ui::Cursor& in, ui::Cursor* out) {
  return mojom::Cursor::Deserialize(mojom::Cursor::Serialize(&in), out);
}

using CursorStructTraitsTest = testing::Test;

}  // namespace

// Test that basic cursor structs are passed correctly across the wire.
TEST_F(CursorStructTraitsTest, TestBuiltIn) {
  for (int i = 0; i < static_cast<int>(ui::CursorType::kCustom); ++i) {
    ui::CursorType type = static_cast<ui::CursorType>(i);
    ui::Cursor input(type);
    input.set_device_scale_factor(1);

    ui::Cursor output;
    ASSERT_TRUE(EchoCursor(input, &output));
    EXPECT_EQ(type, output.native_type());
  }
}

// Test that cursor bitmaps and metadata are passed correctly across the wire.
TEST_F(CursorStructTraitsTest, TestBitmapCursor) {
  ui::Cursor input(ui::CursorType::kCustom);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorRED);

  const gfx::Point kHotspot = gfx::Point(5, 2);
  input.set_custom_hotspot(kHotspot);
  input.set_custom_bitmap(bitmap);

  const float kScale = 2.0f;
  input.set_device_scale_factor(kScale);

  ui::Cursor output;
  EXPECT_TRUE(EchoCursor(input, &output));
  EXPECT_EQ(input, output);

  EXPECT_EQ(ui::CursorType::kCustom, output.native_type());
  EXPECT_EQ(kScale, output.device_scale_factor());
  EXPECT_EQ(kHotspot, output.GetHotspot());

  // Even though the pixel data is the same, the bitmap generation ids differ.
  EXPECT_TRUE(gfx::BitmapsAreEqual(input.GetBitmap(), output.GetBitmap()));
  EXPECT_NE(input.GetBitmap().getGenerationID(),
            output.GetBitmap().getGenerationID());

  // Make a copy of output; the bitmap generation ids should be the same.
  ui::Cursor copy = output;
  EXPECT_EQ(output.GetBitmap().getGenerationID(),
            copy.GetBitmap().getGenerationID());
  EXPECT_EQ(input, output);
}

// Test that empty bitmaps are passed correctly over the wire. This happens when
// renderers relay a custom cursor before the bitmap resource is loaded.
TEST_F(CursorStructTraitsTest, TestEmptyCursor) {
  const gfx::Point kHotspot = gfx::Point(5, 2);
  const float kScale = 2.0f;

  ui::Cursor input(ui::CursorType::kCustom);
  input.set_custom_hotspot(kHotspot);
  input.set_custom_bitmap(SkBitmap());
  input.set_device_scale_factor(kScale);

  ui::Cursor output;
  ASSERT_TRUE(EchoCursor(input, &output));

  EXPECT_TRUE(output.GetBitmap().empty());
}

// Test that various device scale factors are passed correctly over the wire.
TEST_F(CursorStructTraitsTest, TestDeviceScaleFactors) {
  ui::Cursor input(ui::CursorType::kCustom);
  ui::Cursor output;

  for (auto scale : {0.f, 0.525f, 0.75f, 0.9f, 1.f, 2.1f, 2.5f, 3.f, 10.f}) {
    SCOPED_TRACE(testing::Message() << " scale: " << scale);
    input.set_device_scale_factor(scale);
    EXPECT_TRUE(EchoCursor(input, &output));
    EXPECT_EQ(scale, output.device_scale_factor());
  }
}

}  // namespace ui
