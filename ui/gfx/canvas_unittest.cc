// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

#include <limits>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace gfx {

class CanvasTest : public testing::Test {
 protected:
  int GetStringWidth(const char *text) {
    return Canvas::GetStringWidth(base::UTF8ToUTF16(text), font_list_);
  }

  gfx::Size SizeStringInt(const char *text, int width, int line_height) {
    std::u16string text16 = base::UTF8ToUTF16(text);
    int height = 0;
    int flags =
        (text16.find('\n') != std::u16string::npos) ? Canvas::MULTI_LINE : 0;
    Canvas::SizeStringInt(text16, font_list_, &width, &height, line_height,
                          flags);
    return gfx::Size(width, height);
  }

 private:
  FontList font_list_;
};

TEST_F(CanvasTest, StringWidth) {
  EXPECT_GT(GetStringWidth("Test"), 0);
}

TEST_F(CanvasTest, StringWidthEmptyString) {
  EXPECT_EQ(0, GetStringWidth(""));
}

TEST_F(CanvasTest, StringSizeEmptyString) {
  gfx::Size size = SizeStringInt("", 0, 0);
  EXPECT_EQ(0, size.width());
  EXPECT_GT(size.height(), 0);
}

// Verifies GetClipBounds() returns the correct value.
TEST_F(CanvasTest, ClipRectWithScaling) {
  Canvas canvas(gfx::Size(200, 100), 2.25, true);
  canvas.ClipRect(gfx::RectF(100, 0, 20, 1.7f));
  gfx::Rect clip_rect;
  ASSERT_TRUE(canvas.GetClipBounds(&clip_rect));
  // Use Contains() rather than Equals() as skia may extend the rect in certain
  // directions. None-the-less the clip must contain the region we damaged.
  EXPECT_TRUE(clip_rect.Contains(gfx::Rect(100, 0, 20, 2)));
}

TEST_F(CanvasTest, StringSizeWithLineHeight) {
  gfx::Size one_line_size = SizeStringInt("Q", 0, 0);
  gfx::Size four_line_size = SizeStringInt("Q\nQ\nQ\nQ", 1000000, 1000);
  EXPECT_EQ(one_line_size.width(), four_line_size.width());
  EXPECT_EQ(3 * 1000 + one_line_size.height(), four_line_size.height());
}

}  // namespace gfx
