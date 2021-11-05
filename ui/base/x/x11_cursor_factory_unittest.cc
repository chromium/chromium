// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor_factory.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/cursor_loader.h"
#include "ui/base/cursor/mojom/cursor_type.mojom.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
namespace {

using mojom::CursorType;

scoped_refptr<PlatformCursor> LoadInvisibleCursor() {
  CursorLoader cursor_loader;
  Cursor cursor(CursorType::kNone);
  cursor_loader.SetPlatformCursor(&cursor);
  return cursor.platform();
}

}  // namespace

TEST(X11CursorFactoryTest, InvisibleCursor) {
  X11CursorFactory factory;

  // Building an image cursor with an invalid SkBitmap should return the
  // invisible cursor in X11.
  auto invisible_cursor =
      factory.CreateImageCursor({}, SkBitmap(), gfx::Point());
  ASSERT_NE(invisible_cursor, nullptr);
  EXPECT_EQ(invisible_cursor, factory.GetDefaultCursor(CursorType::kNone));
}

TEST(X11CursorFactoryTest, LoadInvisibleCursor) {
  X11CursorFactory cursor_factory;
  // Building an image cursor with an invalid SkBitmap should return the
  // invisible cursor in X11.
  auto invisible_cursor = cursor_factory.GetDefaultCursor(CursorType::kNone);
  ASSERT_NE(invisible_cursor, nullptr);
  EXPECT_EQ(invisible_cursor, LoadInvisibleCursor());
}

}  // namespace ui
