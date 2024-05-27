// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback.h"

#include "base/strings/stringprintf.h"
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
  const std::u16string ascii = u"abc";
  const std::u16string hebrew = u"\x5d0\x5d1\x5d2";
  const std::u16string emoji = u"😋";

  Font fallback;
  EXPECT_TRUE(
      GetFallbackFont(arial, kDefaultApplicationLocale, hebrew, &fallback));
  EXPECT_EQ("Lucida Grande", fallback.GetFontName());
  EXPECT_TRUE(
      GetFallbackFont(arial, kDefaultApplicationLocale, emoji, &fallback));
  EXPECT_EQ("Apple Color Emoji", fallback.GetFontName());
}

TEST(FontFallbackMacTest, GetFallbackFontForEmoji) {
  static struct {
    const char* test_name;
    const wchar_t* text;
  } kEmojiTests[] = {
      {"aries", L"\u2648"},
      {"candle", L"\U0001F56F"},
      {"anchor", L"\u2693"},
      {"grinning_face", L"\U0001F600"},
      {"flag_andorra", L"\U0001F1E6\U0001F1E9"},
      {"woman_man_hands_light", L"\U0001F46B\U0001F3FB"},
      {"hole_text", L"\U0001F573\uFE0E"},
      {"hole_emoji", L"\U0001F573\uFE0F"},
      {"man_judge_medium", L"\U0001F468\U0001F3FD\u200D\u2696\uFE0F"},
      {"woman_turban", L"\U0001F473\u200D\u2640\uFE0F"},
      {"rainbow_flag", L"\U0001F3F3\uFE0F\u200D\U0001F308"},
      {"eye_bubble", L"\U0001F441\uFE0F\u200D\U0001F5E8\uFE0F"},
  };

  Font font;
  for (const auto& test : kEmojiTests) {
    SCOPED_TRACE(
        base::StringPrintf("GetFallbackFontForEmoji [%s]", test.test_name));
    Font fallback;
    EXPECT_TRUE(GetFallbackFont(font, kDefaultApplicationLocale,
                                base::WideToUTF16(test.text), &fallback));
    EXPECT_EQ("Apple Color Emoji", fallback.GetFontName());
  }
}

}  // namespace gfx
