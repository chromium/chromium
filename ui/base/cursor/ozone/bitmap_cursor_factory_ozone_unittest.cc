// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"

#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"

namespace ui {

using mojom::CursorType;

TEST(BitmapCursorFactoryOzoneTest, InvisibleCursor) {
  BitmapCursorFactoryOzone cursor_factory;

  auto cursor = cursor_factory.GetDefaultCursor(CursorType::kNone);
  // The invisible cursor should be a BitmapCursorOzone of type kNone, not
  // nullptr.
  ASSERT_NE(cursor, nullptr);
  EXPECT_EQ(BitmapCursorOzone::FromPlatformCursor(cursor)->type(),
            CursorType::kNone);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST(BitmapCursorFactoryOzoneTest, LacrosUsesDefaultCursorsForCommonTypes) {
  BitmapCursorFactoryOzone factory;

  // Verify some common cursor types.
  auto cursor = factory.GetDefaultCursor(CursorType::kPointer);
  EXPECT_NE(cursor, nullptr);
  EXPECT_EQ(BitmapCursorOzone::FromPlatformCursor(cursor)->type(),
            CursorType::kPointer);

  cursor = factory.GetDefaultCursor(CursorType::kHand);
  EXPECT_NE(cursor, nullptr);
  EXPECT_EQ(BitmapCursorOzone::FromPlatformCursor(cursor)->type(),
            CursorType::kHand);

  cursor = factory.GetDefaultCursor(CursorType::kIBeam);
  EXPECT_NE(cursor, nullptr);
  EXPECT_EQ(BitmapCursorOzone::FromPlatformCursor(cursor)->type(),
            CursorType::kIBeam);
}

TEST(BitmapCursorFactoryOzoneTest, LacrosCustomCursor) {
  BitmapCursorFactoryOzone factory;
  auto cursor = factory.GetDefaultCursor(CursorType::kCustom);
  // Custom cursors don't have a default platform cursor.
  EXPECT_EQ(cursor, nullptr);
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace ui
