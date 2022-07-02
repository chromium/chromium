// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/cursor/cursor_loader.h"

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/cursor/cursor_lookup.h"
#include "ui/aura/cursor/cursors_aura.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_factory.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/skia_util.h"

namespace aura {

namespace {

using CursorLoaderTest = test::AuraTestBase;
using ::ui::mojom::CursorType;

SkBitmap GetTestBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  return bitmap;
}

}  // namespace

TEST_F(CursorLoaderTest, InvisibleCursor) {
  CursorLoader cursor_loader;
  ui::Cursor invisible_cursor(CursorType::kNone);
  cursor_loader.SetPlatformCursor(&invisible_cursor);

  EXPECT_EQ(
      invisible_cursor.platform(),
      ui::CursorFactory::GetInstance()->GetDefaultCursor(CursorType::kNone));
}

// TODO(https://crbug.com/1270302): although this is testing `GetCursorBitmap`
// from cursor_lookup.h, that will be replaced by a method of the same name in
// CursorLoader.
TEST_F(CursorLoaderTest, GetCursorData) {
  ui::RegisterPathProvider();
  base::FilePath ui_test_pak_path =
      base::PathService::CheckedGet(ui::UI_TEST_PAK);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(ui_test_pak_path);

  const ui::Cursor invisible_cursor = CursorType::kNone;
  EXPECT_TRUE(GetCursorBitmap(invisible_cursor).isNull());
  EXPECT_TRUE(GetCursorHotspot(invisible_cursor).IsOrigin());

  const ui::Cursor pointer_cursor = CursorType::kPointer;
  EXPECT_FALSE(GetCursorBitmap(pointer_cursor).isNull());
  EXPECT_TRUE(gfx::BitmapsAreEqual(GetCursorBitmap(pointer_cursor),
                                   GetDefaultBitmap(pointer_cursor)));
  EXPECT_EQ(GetCursorHotspot(pointer_cursor),
            GetDefaultHotspot(pointer_cursor));

  ui::Cursor custom_cursor(CursorType::kCustom);
  const SkBitmap kBitmap = GetTestBitmap();
  constexpr gfx::Point kHotspot = gfx::Point(10, 10);
  custom_cursor.set_custom_bitmap(kBitmap);
  custom_cursor.set_custom_hotspot(kHotspot);
  EXPECT_EQ(GetCursorBitmap(custom_cursor).getGenerationID(),
            kBitmap.getGenerationID());
  EXPECT_EQ(GetCursorHotspot(custom_cursor), kHotspot);
}

}  // namespace aura
