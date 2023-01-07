// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_list.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font_names_testing.h"

namespace gfx {

namespace {

// Helper function for comparing fonts for equality.
std::string FontToString(const Font& font) {
  std::string font_string = font.GetFontName();
  font_string += "|";
  font_string += base::NumberToString(font.GetFontSize());
  int style = font.GetStyle();
  if (style & Font::ITALIC)
    font_string += "|italic";
  if (style & Font::UNDERLINE)
    font_string += "|underline";
  auto weight = font.GetWeight();
  if (weight == Font::Weight::BLACK)
    font_string += "|black";
  else if (weight == Font::Weight::BOLD)
    font_string += "|bold";
  else if (weight == Font::Weight::EXTRA_BOLD)
    font_string += "|extrabold";
  else if (weight == Font::Weight::EXTRA_LIGHT)
    font_string += "|extralight";
  else if (weight == Font::Weight::LIGHT)
    font_string += "|light";
  else if (weight == Font::Weight::MEDIUM)
    font_string += "|medium";
  else if (weight == Font::Weight::NORMAL)
    font_string += "|normal";
  else if (weight == Font::Weight::SEMIBOLD)
    font_string += "|semibold";
  else if (weight == Font::Weight::THIN)
    font_string += "|thin";
  return font_string;
}

}  // namespace

TEST(FontListTest, ParseDescription) {
  std::vector<std::string> families;
  int style = Font::NORMAL;
  int size_pixels = 0;
  Font::Weight weight = Font::Weight::NORMAL;

  // Parse a well-formed description containing styles and a size.
  EXPECT_TRUE(FontList::ParseDescription("Arial,Helvetica,Bold Italic 12px",
                                         &families, &style, &size_pixels,
                                         &weight));
  ASSERT_EQ(2U, families.size());
  EXPECT_EQ("Arial", families[0]);
  EXPECT_EQ("Helvetica", families[1]);
  EXPECT_EQ(Font::ITALIC, style);
  EXPECT_EQ(Font::Weight::BOLD, weight);
  EXPECT_EQ(12, size_pixels);

  // Whitespace should be removed.
  EXPECT_TRUE(FontList::ParseDescription("  Verdana , Italic  Bold   10px ",
                                         &families, &style, &size_pixels,
                                         &weight));
  ASSERT_EQ(1U, families.size());
  EXPECT_EQ("Verdana", families[0]);
  EXPECT_EQ(Font::ITALIC, style);
  EXPECT_EQ(Font::Weight::BOLD, weight);
  EXPECT_EQ(10, size_pixels);

  // Invalid descriptions should be rejected.
  EXPECT_FALSE(
      FontList::ParseDescription("", &families, &style, &size_pixels, &weight));
  EXPECT_FALSE(FontList::ParseDescription("Arial", &families, &style,
                                          &size_pixels, &weight));
  EXPECT_FALSE(FontList::ParseDescription("Arial,12", &families, &style,
                                          &size_pixels, &weight));
  EXPECT_FALSE(FontList::ParseDescription("Arial 12px", &families, &style,
                                          &size_pixels, &weight));
  EXPECT_FALSE(FontList::ParseDescription("Arial,12px,", &families, &style,
                                          &size_pixels, &weight));
  EXPECT_FALSE(FontList::ParseDescription("Arial,0px", &families, &style,
                                          &size_pixels, &weight));
  EXPECT_FALSE(FontList::ParseDescription("Arial,-1px", &families, &style,
                                          &size_pixels, &weight));
  EXPECT_FALSE(FontList::ParseDescription("Arial,foo 12px", &families, &style,
                                          &size_pixels, &weight));
}

TEST(FontListTest, Fonts_FromDescString) {
  // Test init from font name size string.
  FontList font_list = FontList("arial, Courier New, 13px");
  const std::vector<Font>& fonts = font_list.GetFonts();
  ASSERT_EQ(2U, fonts.size());
  EXPECT_EQ("arial|13|normal", FontToString(fonts[0]));
  EXPECT_EQ("Courier New|13|normal", FontToString(fonts[1]));
}

TEST(FontListTest, Fonts_FromDescStringInFlexibleFormat) {
  // Test init from font name size string with flexible format.
  FontList font_list = FontList("  arial   ,   Courier New ,   13px");
  const std::vector<Font>& fonts = font_list.GetFonts();
  ASSERT_EQ(2U, fonts.size());
  EXPECT_EQ("arial|13|normal", FontToString(fonts[0]));
  EXPECT_EQ("Courier New|13|normal", FontToString(fonts[1]));
}

TEST(FontListTest, Fonts_FromDescStringWithStyleInFlexibleFormat) {
  // Test init from font name style size string with flexible format.
  FontList font_list = FontList(
      "  arial  ,  Courier New ,  Bold   "
      "  Italic   13px");
  const std::vector<Font>& fonts = font_list.GetFonts();
  ASSERT_EQ(2U, fonts.size());
  EXPECT_EQ("arial|13|italic|bold", FontToString(fonts[0]));
  EXPECT_EQ("Courier New|13|italic|bold", FontToString(fonts[1]));
}

TEST(FontListTest, Fonts_FromFont) {
  // Test init from Font.
  Font font("Arial", 8);
  FontList font_list = FontList(font);
  const std::vector<Font>& fonts = font_list.GetFonts();
  ASSERT_EQ(1U, fonts.size());
  EXPECT_EQ("Arial|8|normal", FontToString(fonts[0]));
}

TEST(FontListTest, Fonts_FromFontWithNonNormalStyle) {
  // Test init from Font with non-normal style.
  Font font("Arial", 8);
  FontList font_list(font.Derive(2, Font::NORMAL, Font::Weight::BOLD));
  std::vector<Font> fonts = font_list.GetFonts();
  ASSERT_EQ(1U, fonts.size());
  EXPECT_EQ("Arial|10|bold", FontToString(fonts[0]));

  font_list = FontList(font.Derive(-2, Font::ITALIC, Font::Weight::NORMAL));
  fonts = font_list.GetFonts();
  ASSERT_EQ(1U, fonts.size());
  EXPECT_EQ("Arial|6|italic|normal", FontToString(fonts[0]));
}

TEST(FontListTest, Fonts_FromFontVector) {
  // Test init from Font vector.
  Font font("Arial", 8);
  Font font_1("Courier New", 10);
  std::vector<Font> input_fonts;
  input_fonts.push_back(font.Derive(0, Font::NORMAL, Font::Weight::BOLD));
  input_fonts.push_back(font_1.Derive(-2, Font::NORMAL, Font::Weight::BOLD));
  FontList font_list = FontList(input_fonts);
  const std::vector<Font>& fonts = font_list.GetFonts();
  ASSERT_EQ(2U, fonts.size());
  EXPECT_EQ("Arial|8|bold", FontToString(fonts[0]));
  EXPECT_EQ("Courier New|8|bold", FontToString(fonts[1]));
}

TEST(FontListTest, FontDescString_GetStyle) {
  FontList font_list = FontList("Arial,Sans serif, 8px");
  EXPECT_EQ(Font::NORMAL, font_list.GetFontStyle());
  EXPECT_EQ(Font::Weight::NORMAL, font_list.GetFontWeight());

  font_list = FontList("Arial,Sans serif,Bold 8px");
  EXPECT_EQ(Font::NORMAL, font_list.GetFontStyle());
  EXPECT_EQ(Font::Weight::BOLD, font_list.GetFontWeight());

  font_list = FontList("Arial,Sans serif,Italic 8px");
  EXPECT_EQ(Font::ITALIC, font_list.GetFontStyle());
  EXPECT_EQ(Font::Weight::NORMAL, font_list.GetFontWeight());

  font_list = FontList("Arial,Italic Bold 8px");
  EXPECT_EQ(Font::ITALIC, font_list.GetFontStyle());
  EXPECT_EQ(Font::Weight::BOLD, font_list.GetFontWeight());
}

TEST(FontListTest, Fonts_GetStyle) {
  std::vector<Font> fonts;
  fonts.push_back(Font("Arial", 8));
  fonts.push_back(Font("Sans serif", 8));
  FontList font_list = FontList(fonts);
  EXPECT_EQ(Font::NORMAL, font_list.GetFontStyle());
  fonts[0] = fonts[0].Derive(0, Font::ITALIC, Font::Weight::BOLD);
  fonts[1] = fonts[1].Derive(0, Font::ITALIC, Font::Weight::BOLD);
  font_list = FontList(fonts);
  EXPECT_EQ(Font::ITALIC, font_list.GetFontStyle());
  EXPECT_EQ(Font::Weight::BOLD, font_list.GetFontWeight());
}

TEST(FontListTest, Fonts_Derive) {
  std::vector<Font> fonts;
  fonts.push_back(Font("Arial", 8));
  fonts.push_back(Font("Courier New", 8));
  FontList font_list = FontList(fonts);

  FontList derived = font_list.Derive(5, Font::ITALIC, Font::Weight::BOLD);
  const std::vector<Font>& derived_fonts = derived.GetFonts();

  EXPECT_EQ(2U, derived_fonts.size());
  EXPECT_EQ("Arial|13|italic|bold", FontToString(derived_fonts[0]));
  EXPECT_EQ("Courier New|13|italic|bold", FontToString(derived_fonts[1]));

  derived = font_list.Derive(5, Font::UNDERLINE, Font::Weight::BOLD);
  const std::vector<Font>& underline_fonts = derived.GetFonts();

  EXPECT_EQ(2U, underline_fonts.size());
  EXPECT_EQ("Arial|13|underline|bold", FontToString(underline_fonts[0]));
  EXPECT_EQ("Courier New|13|underline|bold", FontToString(underline_fonts[1]));
}

TEST(FontListTest, Fonts_DeriveWithSizeDelta) {
  std::vector<Font> fonts;
  fonts.push_back(
      Font("Arial", 18).Derive(0, Font::ITALIC, Font::Weight::NORMAL));
  fonts.push_back(Font("Courier New", 18)
                      .Derive(0, Font::ITALIC, Font::Weight::NORMAL));
  FontList font_list = FontList(fonts);

  FontList derived = font_list.DeriveWithSizeDelta(-5);
  const std::vector<Font>& derived_fonts = derived.GetFonts();

  EXPECT_EQ(2U, derived_fonts.size());
  EXPECT_EQ("Arial|13|italic|normal", FontToString(derived_fonts[0]));
  EXPECT_EQ("Courier New|13|italic|normal", FontToString(derived_fonts[1]));
}

TEST(FontListTest, Fonts_GetHeight_GetBaseline) {
  // If a font list has only one font, the height and baseline must be the same.
  Font font1(kTestFontName, 16);
  ASSERT_EQ(base::ToLowerASCII(kTestFontName),
            base::ToLowerASCII(font1.GetActualFontName()));
  FontList font_list1(std::string(kTestFontName) + ", 16px");
  EXPECT_EQ(font1.GetHeight(), font_list1.GetHeight());
  EXPECT_EQ(font1.GetBaseline(), font_list1.GetBaseline());

  // If there are two different fonts, the font list returns the max value
  // for the baseline (ascent) and height.
  // NOTE: On most platforms, kCJKFontName has different metrics than
  // kTestFontName, but on Android it does not.
  Font font2(kCJKFontName, 16);
  ASSERT_EQ(base::ToLowerASCII(kCJKFontName),
            base::ToLowerASCII(font2.GetActualFontName()));
  std::vector<Font> fonts;
  fonts.push_back(font1);
  fonts.push_back(font2);
  FontList font_list_mix(fonts);
  // ascent of FontList == max(ascent of Fonts)
  EXPECT_EQ(std::max(font1.GetBaseline(), font2.GetBaseline()),
            font_list_mix.GetBaseline());
  // descent of FontList == max(descent of Fonts)
  EXPECT_EQ(std::max(font1.GetHeight() - font1.GetBaseline(),
                     font2.GetHeight() - font2.GetBaseline()),
            font_list_mix.GetHeight() - font_list_mix.GetBaseline());
}

TEST(FontListTest, Fonts_DeriveWithHeightUpperBound) {
  std::vector<Font> fonts;

  fonts.push_back(Font("Arial", 18));
  fonts.push_back(Font("Sans serif", 18));
  fonts.push_back(Font(kSymbolFontName, 18));
  FontList font_list = FontList(fonts);

  // A smaller upper bound should derive a font list with a smaller height.
  const int height_1 = font_list.GetHeight() - 5;
  FontList derived_1 = font_list.DeriveWithHeightUpperBound(height_1);
  EXPECT_LE(derived_1.GetHeight(), height_1);
  EXPECT_LT(derived_1.GetHeight(), font_list.GetHeight());
  EXPECT_LT(derived_1.GetFontSize(), font_list.GetFontSize());

  // A larger upper bound should not change the height of the font list.
  const int height_2 = font_list.GetHeight() + 5;
  FontList derived_2 = font_list.DeriveWithHeightUpperBound(height_2);
  EXPECT_LE(derived_2.GetHeight(), height_2);
  EXPECT_EQ(font_list.GetHeight(), derived_2.GetHeight());
  EXPECT_EQ(font_list.GetFontSize(), derived_2.GetFontSize());
}

TEST(FontListTest, FirstAvailableOrFirst) {
  EXPECT_TRUE(FontList::FirstAvailableOrFirst("").empty());
  EXPECT_TRUE(FontList::FirstAvailableOrFirst(std::string()).empty());

  EXPECT_EQ("Arial", FontList::FirstAvailableOrFirst("Arial"));
  EXPECT_EQ("not exist", FontList::FirstAvailableOrFirst("not exist"));

  EXPECT_EQ("Arial", FontList::FirstAvailableOrFirst("Arial, not exist"));
  EXPECT_EQ("Arial", FontList::FirstAvailableOrFirst("not exist, Arial"));
  EXPECT_EQ("Arial",
            FontList::FirstAvailableOrFirst("not exist, Arial, not exist"));

  EXPECT_EQ("not exist",
            FontList::FirstAvailableOrFirst("not exist, not exist 2"));

  EXPECT_EQ("Arial", FontList::FirstAvailableOrFirst(", not exist, Arial"));
  EXPECT_EQ("not exist",
            FontList::FirstAvailableOrFirst(", not exist, not exist"));
}

}  // namespace gfx
