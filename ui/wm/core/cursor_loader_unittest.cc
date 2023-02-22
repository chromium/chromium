// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/cursor_loader.h"

#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/cursor_shape_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/skia_util.h"
#include "ui/wm/core/cursor_util.h"

namespace wm {

namespace {

using CursorLoaderTest = ::aura::test::AuraTestBase;
using ::ui::mojom::CursorType;

SkBitmap GetTestBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  return bitmap;
}

std::vector<SkBitmap> GetCursorBitmaps(const ui::Cursor& cursor) {
  auto* cursor_shape_client = aura::client::GetCursorShapeClient();
  EXPECT_NE(cursor_shape_client, nullptr);

  const absl::optional<ui::CursorData> cursor_data =
      cursor_shape_client->GetCursorData(cursor);
  EXPECT_TRUE(cursor_data);
  // CursorData guarantees that bitmaps has at least 1 element.
  return cursor_data->bitmaps;
}

gfx::Point GetCursorHotspot(const ui::Cursor& cursor) {
  auto* cursor_shape_client = aura::client::GetCursorShapeClient();
  EXPECT_NE(cursor_shape_client, nullptr);

  const absl::optional<ui::CursorData> cursor_data =
      cursor_shape_client->GetCursorData(cursor);
  EXPECT_TRUE(cursor_data);
  return cursor_data->hotspot;
}

}  // namespace

TEST_F(CursorLoaderTest, InvisibleCursor) {
  auto* cursor_loader =
      static_cast<CursorLoader*>(aura::client::GetCursorShapeClient());
  ui::Cursor invisible_cursor(CursorType::kNone);
  cursor_loader->SetPlatformCursor(&invisible_cursor);

  EXPECT_EQ(
      invisible_cursor.platform(),
      ui::CursorFactory::GetInstance()->GetDefaultCursor(CursorType::kNone));
}

TEST_F(CursorLoaderTest, GetCursorData) {
  // Make sure we always use the fallback cursors, so the test works the same
  // in all platforms.
  CursorLoader cursor_loader(/*use_platform_cursors=*/false);
  aura::client::SetCursorShapeClient(&cursor_loader);

  const ui::CursorSize kDefaultSize = ui::CursorSize::kNormal;
  const float kDefaultScale = 1.0f;
  const display::Display::Rotation kDefaultRotation =
      display::Display::ROTATE_0;

  const ui::Cursor invisible_cursor = CursorType::kNone;
  EXPECT_TRUE(GetCursorBitmaps(invisible_cursor)[0].isNull());
  EXPECT_TRUE(GetCursorHotspot(invisible_cursor).IsOrigin());

  const ui::Cursor pointer_cursor = CursorType::kPointer;
  EXPECT_EQ(GetCursorBitmaps(pointer_cursor).size(), 1u);
  EXPECT_FALSE(GetCursorBitmaps(pointer_cursor)[0].isNull());
  const auto pointer_cursor_data = GetCursorData(
      CursorType::kPointer, kDefaultSize, kDefaultScale, kDefaultRotation);
  ASSERT_TRUE(pointer_cursor_data);
  ASSERT_EQ(pointer_cursor_data->bitmaps.size(), 1u);
  EXPECT_TRUE(gfx::BitmapsAreEqual(GetCursorBitmaps(pointer_cursor)[0],
                                   pointer_cursor_data->bitmaps[0]));
  EXPECT_FALSE(GetCursorHotspot(pointer_cursor).IsOrigin());
  EXPECT_EQ(GetCursorHotspot(pointer_cursor), pointer_cursor_data->hotspot);

  const ui::Cursor wait_cursor = CursorType::kWait;
  EXPECT_FALSE(GetCursorBitmaps(wait_cursor)[0].isNull());
  const auto wait_cursor_data = GetCursorData(CursorType::kWait, kDefaultSize,
                                              kDefaultScale, kDefaultRotation);
  ASSERT_TRUE(wait_cursor_data);
  ASSERT_GT(wait_cursor_data->bitmaps.size(), 1u);
  ASSERT_EQ(GetCursorBitmaps(wait_cursor).size(),
            wait_cursor_data->bitmaps.size());
  for (size_t i = 0; i < wait_cursor_data->bitmaps.size(); i++) {
    EXPECT_TRUE(gfx::BitmapsAreEqual(GetCursorBitmaps(wait_cursor)[i],
                                     wait_cursor_data->bitmaps[i]));
  }
  EXPECT_FALSE(GetCursorHotspot(wait_cursor).IsOrigin());
  EXPECT_EQ(GetCursorHotspot(wait_cursor), wait_cursor_data->hotspot);

  const SkBitmap kBitmap = GetTestBitmap();
  constexpr gfx::Point kHotspot = gfx::Point(10, 10);
  const ui::Cursor custom_cursor = ui::Cursor::NewCustom(kBitmap, kHotspot);
  EXPECT_EQ(GetCursorBitmaps(custom_cursor)[0].getGenerationID(),
            kBitmap.getGenerationID());
  EXPECT_EQ(GetCursorHotspot(custom_cursor), kHotspot);
}

// Test the cursor image cache when fallbacks for system cursors are used.
TEST_F(CursorLoaderTest, ImageCursorCache) {
  CursorLoader cursor_loader(/*use_platform_cursors=*/false);
  ui::Cursor cursor(CursorType::kPointer);
  cursor_loader.SetPlatformCursor(&cursor);

  // CursorLoader should keep a ref in its cursor cache.
  auto platform_cursor = cursor.platform();
  cursor.SetPlatformCursor(nullptr);
  EXPECT_FALSE(platform_cursor->HasOneRef());

  // Invalidate the cursor cache by changing the rotation.
  cursor_loader.SetDisplayData(display::Display::ROTATE_90,
                               cursor_loader.scale());
  EXPECT_TRUE(platform_cursor->HasOneRef());

  // Invalidate the cursor cache by changing the scale.
  cursor_loader.SetPlatformCursor(&cursor);
  platform_cursor = cursor.platform();
  cursor.SetPlatformCursor(nullptr);
  EXPECT_FALSE(platform_cursor->HasOneRef());
  cursor_loader.SetDisplayData(cursor_loader.rotation(),
                               cursor_loader.scale() * 2);
  EXPECT_TRUE(platform_cursor->HasOneRef());
}

}  // namespace wm
