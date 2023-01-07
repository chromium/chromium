// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/win_cursor_factory.h"

#include "base/win/windows_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/win/win_cursor.h"

namespace ui {

namespace {

using mojom::CursorType;

}  // namespace

TEST(WinCursorFactoryTest, InvisibleCursor) {
  WinCursorFactory factory;
  auto invisible_cursor = factory.GetDefaultCursor(CursorType::kNone);
  ASSERT_NE(invisible_cursor, nullptr);
  EXPECT_EQ(WinCursor::FromPlatformCursor(invisible_cursor)->hcursor(),
            nullptr);
}

}  // namespace ui
