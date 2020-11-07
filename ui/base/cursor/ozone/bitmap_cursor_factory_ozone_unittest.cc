// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"

#include "base/optional.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"

namespace ui {

TEST(BitmapCursorFactoryOzoneTest, InvisibleCursor) {
  BitmapCursorFactoryOzone cursor_factory;

  base::Optional<PlatformCursor> cursor =
      cursor_factory.GetDefaultCursor(mojom::CursorType::kNone);
  // The invisible cursor should be nullptr, not base::nullopt.
  ASSERT_TRUE(cursor.has_value());
  EXPECT_EQ(cursor, nullptr);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST(BitmapCursorFactoryOzoneTest, LacrosUsesDefaultCursorsForCommonTypes) {
  BitmapCursorFactoryOzone factory;

  // Verify some common cursor types.
  base::Optional<PlatformCursor> cursor =
      factory.GetDefaultCursor(mojom::CursorType::kPointer);
  ASSERT_TRUE(cursor.has_value());
  EXPECT_NE(cursor, nullptr);
  factory.UnrefImageCursor(cursor.value());

  cursor = factory.GetDefaultCursor(mojom::CursorType::kHand);
  ASSERT_TRUE(cursor.has_value());
  EXPECT_NE(cursor, nullptr);
  factory.UnrefImageCursor(cursor.value());

  cursor = factory.GetDefaultCursor(mojom::CursorType::kIBeam);
  ASSERT_TRUE(cursor.has_value());
  EXPECT_NE(cursor, nullptr);
  factory.UnrefImageCursor(cursor.value());
}

TEST(BitmapCursorFactoryOzoneTest, LacrosCustomCursor) {
  BitmapCursorFactoryOzone factory;
  base::Optional<PlatformCursor> cursor =
      factory.GetDefaultCursor(mojom::CursorType::kCustom);
  // Custom cursors don't have a default platform cursor.
  EXPECT_FALSE(cursor.has_value());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace ui
