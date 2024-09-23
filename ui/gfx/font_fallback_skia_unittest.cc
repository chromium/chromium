// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font_fallback.h"

namespace gfx {

namespace {

const wchar_t* kFallbackFontTests[] = {
    L"\u0540\u0541",  // Armenian,
    L"\u0631\u0632",  // Arabic
    L"\u0915\u093f",  // Devanagari
    L"\u5203\u5204",  // CJK Unified Ideograph
};

const char kDefaultApplicationLocale[] = "us-en";

}  // namespace

TEST(FontFallbackSkiaTest, EmptyStringFallback) {
  Font base_font;
  Font fallback_font;
  bool result = GetFallbackFont(base_font, kDefaultApplicationLocale,
                                std::u16string_view(), &fallback_font);
  EXPECT_FALSE(result);
}

TEST(FontFallbackSkiaTest, FontFallback) {
  for (const auto* test : kFallbackFontTests) {
    Font base_font;
    Font fallback_font;
    std::u16string text = base::WideToUTF16(test);

    if (!GetFallbackFont(base_font, kDefaultApplicationLocale, text,
                         &fallback_font)) {
      ADD_FAILURE() << "Font fallback failed: '" << text << "'";
    }
  }
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)
// TODO(sergeyu): Fuchsia doesn't not support locale for font fallbacks.
// TODO(etienneb): Android doesn't allow locale override, unless the language
//                 is added in the system UI.
TEST(FontFallbackSkiaTest, CJKLocaleFallback) {
  // Han unification is an effort to map multiple character sets of the CJK
  // languages into a single set of unified characters. Han characters are a
  // common feature of written Chinese (hanzi), Japanese (kanji), and Korean
  // (hanja). The same text will be rendered using a different font based on
  // locale.
  const std::u16string kCJKTest = u"\u8AA4\u904E\u9AA8";
  Font base_font;

  Font fallback_font_zh_cn;
  Font fallback_font_zh_tw;
  Font fallback_font_zh_hk;
  EXPECT_TRUE(
      GetFallbackFont(base_font, "zh-CN", kCJKTest, &fallback_font_zh_cn));
  EXPECT_TRUE(
      GetFallbackFont(base_font, "zh-TW", kCJKTest, &fallback_font_zh_tw));
  EXPECT_TRUE(
      GetFallbackFont(base_font, "zh-HK", kCJKTest, &fallback_font_zh_hk));
  EXPECT_EQ(fallback_font_zh_cn.GetFontName(),
            fallback_font_zh_tw.GetFontName());
  EXPECT_EQ(fallback_font_zh_cn.GetFontName(),
            fallback_font_zh_hk.GetFontName());

  Font fallback_font_ja;
  Font fallback_font_ja_jp;
  EXPECT_TRUE(GetFallbackFont(base_font, "ja", kCJKTest, &fallback_font_ja));
  EXPECT_TRUE(
      GetFallbackFont(base_font, "ja-JP", kCJKTest, &fallback_font_ja_jp));
  EXPECT_EQ(fallback_font_ja.GetFontName(), fallback_font_ja_jp.GetFontName());

  Font fallback_font_ko;
  Font fallback_font_ko_kr;
  EXPECT_TRUE(GetFallbackFont(base_font, "ko", kCJKTest, &fallback_font_ko));
  EXPECT_TRUE(
      GetFallbackFont(base_font, "ko-KR", kCJKTest, &fallback_font_ko_kr));
  EXPECT_EQ(fallback_font_ko.GetFontName(), fallback_font_ko_kr.GetFontName());

  // The three fonts must not be the same.
  EXPECT_NE(fallback_font_zh_cn.GetFontName(), fallback_font_ja.GetFontName());
  EXPECT_NE(fallback_font_zh_cn.GetFontName(), fallback_font_ko.GetFontName());
  EXPECT_NE(fallback_font_ja.GetFontName(), fallback_font_ko.GetFontName());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_FUCHSIA)

}  // namespace gfx
