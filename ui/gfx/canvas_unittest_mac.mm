// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/canvas.h"

#include <cmath>

#import <Cocoa/Cocoa.h>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"

namespace gfx {

namespace {

// Returns the pixel width of the string via calling the native method
// -sizeWithAttributes.
float GetStringNativeWidth(const base::string16& text,
                           const FontList& font_list) {
  NSFont* native_font = font_list.GetPrimaryFont().GetNativeFont();
  NSString* ns_string = base::SysUTF16ToNSString(text);
  NSDictionary* attributes = @{NSFontAttributeName : native_font};
  return [ns_string sizeWithAttributes:attributes].width;
}

}  // namespace

class CanvasTestMac : public testing::Test {
 protected:
  // Compare the size returned by Canvas::SizeStringInt to the size generated
  // by the platform-specific version in CanvasMac_SizeStringInt. Will generate
  // expectation failure on any mismatch. Only works for single-line text
  // without specified line height, since that is all the platform
  // implementation supports.
  void CompareSizes(const char* text) {
    const float kReallyLargeNumber = 12345678;
    FontList font_list(font_);
    base::string16 text16 = base::UTF8ToUTF16(text);

    float mac_width = GetStringNativeWidth(text16, font_list);
    int mac_height = font_list.GetHeight();

    float canvas_width = kReallyLargeNumber;
    float canvas_height = kReallyLargeNumber;
    Canvas::SizeStringFloat(text16, font_list, &canvas_width, &canvas_height, 0,
                            0, Typesetter::NATIVE);

    EXPECT_NE(kReallyLargeNumber, mac_width) << "no width for " << text;
    EXPECT_NE(kReallyLargeNumber, mac_height) << "no height for " << text;
    EXPECT_EQ(mac_width, canvas_width) << " width for " << text;
    // FontList::GetHeight returns a truncated height.
    EXPECT_EQ(mac_height,
              static_cast<int>(canvas_height)) << " height for " << text;
  }

 private:
  Font font_;
};

// Tests that Canvas' SizeStringFloat yields result consistent with a native
// implementation when using Typesetter::NATIVE.
TEST_F(CanvasTestMac, StringSizeIdenticalForSkia) {
  CompareSizes("");
  CompareSizes("Foo");
  CompareSizes("Longword");
  CompareSizes("This is a complete sentence.");
}

TEST_F(CanvasTestMac, FractionalWidth) {
  const float kReallyLargeNumber = 12345678;
  float width = kReallyLargeNumber;
  float height = kReallyLargeNumber;

  FontList font_list;
  Canvas::SizeStringFloat(base::UTF8ToUTF16("Test"), font_list, &width, &height,
                          0, 0, Typesetter::NATIVE);

  EXPECT_GT(width, static_cast<int>(width));
}

}  // namespace gfx
