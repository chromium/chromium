// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_cursor_factory.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/x/x11_cursor.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

TEST(X11CursorFactoryTest, InvisibleRefcount) {
  X11CursorFactory factory;

  // Building an image cursor with an invalid SkBitmap should return the
  // invisible cursor in X11. The invisible cursor instance should have more
  // than a single reference since the factory should hold a reference and
  // CreateImageCursor should return an incremented refcount.
  auto* invisible_cursor = static_cast<X11Cursor*>(
      factory.CreateImageCursor({}, SkBitmap(), gfx::Point()));
  ASSERT_FALSE(invisible_cursor->HasOneRef());

  // Release our refcount on the cursor
  factory.UnrefImageCursor(invisible_cursor);

  // The invisible cursor should still exist.
  EXPECT_TRUE(invisible_cursor->HasOneRef());
}

}  // namespace ui
