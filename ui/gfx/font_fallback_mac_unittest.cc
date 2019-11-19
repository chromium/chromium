// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"

namespace gfx {

namespace {
const char kDefaultApplicationLocale[] = "us-en";
}  // namespace

// A targeted test for GetFallbackFonts on Mac. It uses a system API that
// only became publicly available in the 10.8 SDK. This test is to ensure it
// behaves sensibly on all supported OS versions.
TEST(FontFallbackMacTest, GetFallbackFonts) {
  Font font("Arial", 12);
  std::vector<Font> fallback_fonts = GetFallbackFonts(font);
  // If there is only one fallback, it means the only fallback is the font
  // itself.
  EXPECT_LT(1u, fallback_fonts.size());
}

// Sanity check GetFallbackFont() behavior on Mac. This test makes assumptions
// about font properties and availability on specific macOS versions.
TEST(FontFallbackMacTest, GetFallbackFont) {
  Font arial("Helvetica", 12);
  const base::string16 ascii = base::ASCIIToUTF16("abc");
  const base::string16 hebrew = base::WideToUTF16(L"\x5d0\x5d1\x5d2");
  const base::string16 emoji = base::UTF8ToUTF16("😋");

  gfx::Font fallback;
  EXPECT_FALSE(
      GetFallbackFont(arial, kDefaultApplicationLocale, ascii, &fallback));
  EXPECT_TRUE(
      GetFallbackFont(arial, kDefaultApplicationLocale, hebrew, &fallback));
  EXPECT_EQ("Lucida Grande", fallback.GetFontName());
  EXPECT_TRUE(
      GetFallbackFont(arial, kDefaultApplicationLocale, emoji, &fallback));
  EXPECT_EQ("Apple Color Emoji", fallback.GetFontName());
}

}  // namespace gfx
