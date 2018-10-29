// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_win.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gfx {

TEST(FontFallbackWinTest, ParseFontLinkEntry) {
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

TEST(FontFallbackWinTest, ParseFontFamilyString) {
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

TEST(FontFallbackWinTest, FontFallback) {
  // Needed to bypass DCHECK in GetFallbackFont.
  base::MessageLoopForUI message_loop;

  Font base_font("Segoe UI", 14);
  Font fallback_font;
  bool result = GetFallbackFont(base_font, L"abc", 3, &fallback_font);
  EXPECT_TRUE(result);
  EXPECT_STREQ("Segoe UI", fallback_font.GetFontName().c_str());
}

TEST(FontFallbackWinTest, EmptyStringFallback) {
  // Needed to bypass DCHECK in GetFallbackFont.
  base::MessageLoopForUI message_loop;

  Font base_font;
  Font fallback_font;
  bool result = GetFallbackFont(base_font, L"", 0, &fallback_font);
  EXPECT_FALSE(result);
}

}  // namespace gfx
