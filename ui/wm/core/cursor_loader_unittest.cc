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
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/skia_util.h"
#include "ui/wm/core/cursors_aura.h"

namespace wm {

namespace {

using CursorLoaderTest = ::aura::test::AuraTestBase;
using ::ui::mojom::CursorType;

SkBitmap GetTestBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  return bitmap;
}

SkBitmap GetCursorBitmap(const ui::Cursor& cursor) {
  auto* cursor_shape_client = aura::client::GetCursorShapeClient();
  EXPECT_NE(cursor_shape_client, nullptr);

  const absl::optional<ui::CursorData> cursor_data =
      cursor_shape_client->GetCursorData(cursor);
  EXPECT_TRUE(cursor_data);
  // CursorData guarantees that bitmaps has at least 1 element.
  return cursor_data->bitmaps[0];
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
  const ui::Cursor invisible_cursor = CursorType::kNone;
  EXPECT_TRUE(GetCursorBitmap(invisible_cursor).isNull());
  EXPECT_TRUE(GetCursorHotspot(invisible_cursor).IsOrigin());

  const ui::Cursor pointer_cursor = CursorType::kPointer;
  EXPECT_FALSE(GetCursorBitmap(pointer_cursor).isNull());
  EXPECT_TRUE(gfx::BitmapsAreEqual(GetCursorBitmap(pointer_cursor),
                                   GetDefaultBitmap(pointer_cursor)));
  EXPECT_EQ(GetCursorHotspot(pointer_cursor),
            GetDefaultHotspot(pointer_cursor));

  const ui::Cursor wait_cursor = CursorType::kPointer;
  EXPECT_FALSE(GetCursorBitmap(wait_cursor).isNull());
  EXPECT_TRUE(gfx::BitmapsAreEqual(GetCursorBitmap(wait_cursor),
                                   GetDefaultBitmap(wait_cursor)));
  EXPECT_EQ(GetCursorHotspot(wait_cursor), GetDefaultHotspot(wait_cursor));

  ui::Cursor custom_cursor(CursorType::kCustom);
  const SkBitmap kBitmap = GetTestBitmap();
  constexpr gfx::Point kHotspot = gfx::Point(10, 10);
  custom_cursor.set_custom_bitmap(kBitmap);
  custom_cursor.set_custom_hotspot(kHotspot);
  EXPECT_EQ(GetCursorBitmap(custom_cursor).getGenerationID(),
            kBitmap.getGenerationID());
  EXPECT_EQ(GetCursorHotspot(custom_cursor), kHotspot);
}

}  // namespace wm
