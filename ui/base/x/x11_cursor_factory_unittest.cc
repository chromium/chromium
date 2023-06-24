// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor_factory.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/cursor/platform_cursor.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/gfx/geometry/point.h"

namespace ui {
namespace {

using mojom::CursorType;

}  // namespace

TEST(X11CursorFactoryTest, InvisibleCursor) {
  X11CursorFactory factory;

  // Building an image cursor with an invalid SkBitmap should return the
  // invisible cursor in X11.
  auto invisible_cursor =
      factory.CreateImageCursor({}, SkBitmap(), gfx::Point(), 1.0f);
  ASSERT_NE(invisible_cursor, nullptr);
  EXPECT_EQ(invisible_cursor, factory.GetDefaultCursor(CursorType::kNone));
}

}  // namespace ui
