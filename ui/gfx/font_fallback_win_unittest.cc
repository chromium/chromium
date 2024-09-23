// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_win.h"

#include <string_view>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

namespace {

const char kDefaultApplicationLocale[] = "us-en";

class FontFallbackWinTest : public testing::Test {
 public:
  FontFallbackWinTest() = default;

  FontFallbackWinTest(const FontFallbackWinTest&) = delete;
  FontFallbackWinTest& operator=(const FontFallbackWinTest&) = delete;

 private:
  // Needed to bypass DCHECK in GetFallbackFont.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI};
};

}  // namespace

TEST_F(FontFallbackWinTest, ParseFontLinkEntry) {
  std::string file;
  std::string font;

  internal::ParseFontLinkEntry("TAHOMA.TTF", &file, &font);
  EXPECT_EQ("TAHOMA.TTF", file);
  EXPECT_EQ("", font);

  internal::ParseFontLinkEntry("MSGOTHIC.TTC,MS UI Gothic", &file, &font);
  EXPECT_EQ("MSGOTHIC.TTC", file);
  EXPECT_EQ("MS UI Gothic", font);

  internal::ParseFontLinkEntry("MALGUN.TTF,128,96", &file, &font);
  EXPECT_EQ("MALGUN.TTF", file);
  EXPECT_EQ("", font);

  internal::ParseFontLinkEntry("MEIRYO.TTC,Meiryo,128,85", &file, &font);
  EXPECT_EQ("MEIRYO.TTC", file);
  EXPECT_EQ("Meiryo", font);
}

TEST_F(FontFallbackWinTest, ParseFontFamilyString) {
  std::vector<std::string> font_names;

  internal::ParseFontFamilyString("Times New Roman (TrueType)", &font_names);
  ASSERT_EQ(1U, font_names.size());
  EXPECT_EQ("Times New Roman", font_names[0]);
  font_names.clear();

  internal::ParseFontFamilyString("Cambria & Cambria Math (TrueType)",
                                  &font_names);
  ASSERT_EQ(2U, font_names.size());
  EXPECT_EQ("Cambria", font_names[0]);
  EXPECT_EQ("Cambria Math", font_names[1]);
  font_names.clear();

  internal::ParseFontFamilyString(
      "Meiryo & Meiryo Italic & Meiryo UI & Meiryo UI Italic (TrueType)",
      &font_names);
  ASSERT_EQ(4U, font_names.size());
  EXPECT_EQ("Meiryo", font_names[0]);
  EXPECT_EQ("Meiryo Italic", font_names[1]);
  EXPECT_EQ("Meiryo UI", font_names[2]);
  EXPECT_EQ("Meiryo UI Italic", font_names[3]);
}

TEST_F(FontFallbackWinTest, EmptyStringFallback) {
  Font base_font;
  Font fallback_font;
  bool result = GetFallbackFont(base_font, kDefaultApplicationLocale,
                                std::u16string_view(), &fallback_font);
  EXPECT_FALSE(result);
}

TEST_F(FontFallbackWinTest, NulTerminatedStringPiece) {
  Font base_font;
  Font fallback_font;
  // Multiple ending NUL characters.
  const char16_t kTest1[] = {0x0540, 0x0541, 0, 0, 0};
  EXPECT_FALSE(GetFallbackFont(base_font, kDefaultApplicationLocale,
                               std::u16string_view(kTest1, std::size(kTest1)),
                               &fallback_font));
  // No ending NUL character.
  const char16_t kTest2[] = {0x0540, 0x0541};
  EXPECT_TRUE(GetFallbackFont(base_font, kDefaultApplicationLocale,
                              std::u16string_view(kTest2, std::size(kTest2)),
                              &fallback_font));

  // NUL only characters.
  const char16_t kTest3[] = {0, 0, 0};
  EXPECT_FALSE(GetFallbackFont(base_font, kDefaultApplicationLocale,
                               std::u16string_view(kTest3, std::size(kTest3)),
                               &fallback_font));
}

TEST_F(FontFallbackWinTest, CJKLocaleFallback) {
  // Han unification is an effort to map multiple character sets of the CJK
  // languages into a single set of unified characters. Han characters are a
  // common feature of written Chinese (hanzi), Japanese (kanji), and Korean
  // (hanja). The same text will be rendered using a different font based on
  // locale.
  const char16_t kCJKTest[] = u"\u8AA4\u904E\u9AA8";
  Font base_font;
  Font fallback_font;

  EXPECT_TRUE(GetFallbackFont(base_font, "zh-CN", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Microsoft YaHei UI");

  EXPECT_TRUE(GetFallbackFont(base_font, "zh-TW", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Microsoft JhengHei UI");

  EXPECT_TRUE(GetFallbackFont(base_font, "zh-HK", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Microsoft JhengHei UI");

  EXPECT_TRUE(GetFallbackFont(base_font, "ja", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Yu Gothic UI");

  EXPECT_TRUE(GetFallbackFont(base_font, "ja-JP", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Yu Gothic UI");

  EXPECT_TRUE(GetFallbackFont(base_font, "ko", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Malgun Gothic");

  EXPECT_TRUE(GetFallbackFont(base_font, "ko-KR", kCJKTest, &fallback_font));
  EXPECT_EQ(fallback_font.GetFontName(), "Malgun Gothic");
}

}  // namespace gfx
