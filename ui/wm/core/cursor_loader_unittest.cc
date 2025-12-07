// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_loader.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"
#include "ui/wm/core/cursor_util.h"

namespace wm {

using ::ui::mojom::CursorType;

TEST(CursorLoaderTest, InvisibleCursor) {
  CursorLoader cursor_loader;
  ui::Cursor invisible_cursor(CursorType::kNone);
  cursor_loader.SetPlatformCursor(&invisible_cursor);

  EXPECT_EQ(
      invisible_cursor.platform(),
      ui::CursorFactory::GetInstance()->GetDefaultCursor(CursorType::kNone));
}

TEST(CursorLoaderTest, GetCursorDataForDefaultCursors) {
  // Make sure we always use the fallback cursors, so the test works the same
  // in all platforms.
  CursorLoader cursor_loader(/*use_platform_cursors=*/false);

  display::Display display = display::Display::GetDefaultDisplay();

  for (const float scale : {0.8f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f}) {
    SCOPED_TRACE(testing::Message() << "scale " << scale);
    display.set_device_scale_factor(scale);
    cursor_loader.SetDisplay(display);
    const float resource_scale = ui::GetScaleForResourceScaleFactor(
        ui::GetSupportedResourceScaleFactor(scale));
    for (const ui::CursorSize cursor_size :
         {ui::CursorSize::kNormal, ui::CursorSize::kLarge}) {
      SCOPED_TRACE(testing::Message()
                   << "size " << static_cast<int>(cursor_size));
      cursor_loader.SetSize(cursor_size);

      const ui::Cursor invisible_cursor = CursorType::kNone;
      std::optional<ui::CursorData> cursor_loader_data =
          cursor_loader.GetCursorData(invisible_cursor);
      ASSERT_TRUE(cursor_loader_data);
      EXPECT_TRUE(cursor_loader_data->bitmaps[0].isNull());
      EXPECT_TRUE(cursor_loader_data->hotspot.IsOrigin());

      for (const ui::Cursor cursor :
           {CursorType::kPointer, CursorType::kWait}) {
        SCOPED_TRACE(cursor.type());
        for (const SkColor cursor_color :
             {ui::kDefaultCursorColor, SK_ColorRED, SK_ColorGREEN}) {
          SCOPED_TRACE(testing::Message()
                       << "color " << static_cast<int>(cursor_color));
          cursor_loader.SetColor(cursor_color);
          cursor_loader_data = cursor_loader.GetCursorData(cursor);
          ASSERT_TRUE(cursor_loader_data);

          // `cursor_loader.large_cursor_size_in_dip()` only takes into effect
          // when cursor size is large.
          auto target_cursor_size_in_px =
              cursor_size == ui::CursorSize::kNormal
                  ? std::nullopt
                  : std::make_optional(
                        cursor_loader.large_cursor_size_in_dip() * scale);
          const auto cursor_data = GetCursorData(
              cursor.type(), resource_scale, target_cursor_size_in_px,
              display.panel_rotation(), cursor_color);
          ASSERT_TRUE(cursor_data);
          ASSERT_EQ(cursor_loader_data->bitmaps.size(),
                    cursor_data->bitmaps.size());
          for (size_t i = 0; i < cursor_data->bitmaps.size(); i++) {
            ASSERT_TRUE(gfx::BitmapsAreEqual(cursor_loader_data->bitmaps[i],
                                             cursor_data->bitmaps[i]));
          }
          EXPECT_EQ(cursor_loader_data->hotspot, cursor_data->hotspot);
        }
      }
    }
  }
}

TEST(CursorLoaderTest, GetCursorDataForCustomCursors) {
  CursorLoader cursor_loader(/*use_platform_cursors=*/false);

  static constexpr int large_cursor_size_in_dip = 50;
  cursor_loader.SetLargeCursorSizeInDip(large_cursor_size_in_dip);
  display::Display display = display::Display::GetDefaultDisplay();

  for (const float scale : {0.8f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f}) {
    SCOPED_TRACE(testing::Message() << "scale " << scale);
    display.set_device_scale_factor(scale);
    cursor_loader.SetDisplay(display);
    for (const ui::CursorSize cursor_size :
         {ui::CursorSize::kNormal, ui::CursorSize::kLarge}) {
      SCOPED_TRACE(testing::Message()
                   << "size " << static_cast<int>(cursor_size));
      cursor_loader.SetSize(cursor_size);

      // Custom cursor.
      const SkBitmap custom_bitmap = gfx::test::CreateBitmap(50, 50);
      static constexpr gfx::Point custom_hotspot = gfx::Point(10, 10);
      static constexpr float custom_scale = 2.0f;
      const ui::Cursor custom_cursor =
          ui::Cursor::NewCustom(custom_bitmap, custom_hotspot, custom_scale);

      // The bitmap pixels of a custom cursor will only update if
      // (1) the cursor size is large cursor and needs to be scaled,
      // (2) the cursor color is changed to a non-default value.
      if (cursor_size == ui::CursorSize::kNormal ||
          (cursor_size == ui::CursorSize::kLarge &&
           static_cast<int>(large_cursor_size_in_dip * scale) ==
               custom_bitmap.dimensions().height())) {
        // Cursor doesn't need to be scaled.
        cursor_loader.SetColor(ui::kDefaultCursorColor);
        std::optional<ui::CursorData> cursor_data =
            cursor_loader.GetCursorData(custom_cursor);
        ASSERT_TRUE(cursor_data);
        // Bitmap pixels won't update since the cursor color is the default
        // value.
        EXPECT_EQ(cursor_data->bitmaps[0].getGenerationID(),
                  custom_bitmap.getGenerationID());
        EXPECT_EQ(cursor_data->hotspot, custom_hotspot);

        cursor_loader.SetColor(SK_ColorRED);
        cursor_data = cursor_loader.GetCursorData(custom_cursor);
        ASSERT_TRUE(cursor_data);
        // Bitmap pixels should update since the cursor color is changed.
        EXPECT_NE(cursor_data->bitmaps[0].getGenerationID(),
                  custom_bitmap.getGenerationID());
        EXPECT_EQ(cursor_data->hotspot, custom_hotspot);
      } else {
        // Cursor needs to be scaled.
        cursor_loader.SetColor(ui::kDefaultCursorColor);
        std::optional<ui::CursorData> cursor_data =
            cursor_loader.GetCursorData(custom_cursor);
        ASSERT_TRUE(cursor_data);

        int large_cursor_size_in_px = large_cursor_size_in_dip * scale;
        float rescale = static_cast<float>(large_cursor_size_in_px) /
                        static_cast<float>(custom_bitmap.dimensions().height());
        EXPECT_EQ(cursor_data->bitmaps[0].dimensions().height(),
                  large_cursor_size_in_px);
        EXPECT_EQ(cursor_data->hotspot,
                  gfx::ScaleToFlooredPoint(custom_hotspot, rescale));
      }
    }
  }
}

// Test the cursor image cache when fallbacks for system cursors are used.
TEST(CursorLoaderTest, ImageCursorCache) {
  display::Display display = display::Display::GetDefaultDisplay();
  CursorLoader cursor_loader(/*use_platform_cursors=*/false);
  cursor_loader.SetDisplay(display);

  ui::Cursor cursor(CursorType::kPointer);
  cursor_loader.SetPlatformCursor(&cursor);

  // CursorLoader should keep a ref in its cursor cache.
  auto platform_cursor = cursor.platform();
  cursor.SetPlatformCursor(nullptr);
  EXPECT_FALSE(platform_cursor->HasOneRef());

  // Invalidate the cursor cache by changing the rotation.
  display.set_panel_rotation(display::Display::ROTATE_90);
  cursor_loader.SetDisplay(display);
  EXPECT_TRUE(platform_cursor->HasOneRef());

  // Invalidate the cursor cache by changing the scale.
  cursor_loader.SetPlatformCursor(&cursor);
  platform_cursor = cursor.platform();
  cursor.SetPlatformCursor(nullptr);
  EXPECT_FALSE(platform_cursor->HasOneRef());
  display.set_device_scale_factor(display.device_scale_factor() * 2);
  cursor_loader.SetDisplay(display);
  EXPECT_TRUE(platform_cursor->HasOneRef());
}

}  // namespace wm
