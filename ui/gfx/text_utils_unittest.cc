// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/text_utils.h"

#include <stddef.h>

#include <vector>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
namespace {

TEST(TextUtilsTest, GetStringWidth) {
  FontList font_list;
  EXPECT_EQ(GetStringWidth(std::u16string(), font_list), 0);
  EXPECT_GT(GetStringWidth(u"a", font_list),
            GetStringWidth(std::u16string(), font_list));
  EXPECT_GT(GetStringWidth(u"ab", font_list), GetStringWidth(u"a", font_list));
  EXPECT_GT(GetStringWidth(u"abc", font_list),
            GetStringWidth(u"ab", font_list));
}

TEST(TextUtilsTest, GetStringSize) {
  std::vector<std::u16string> strings{
      std::u16string(),
      u"a",
      u"abc",
  };

  FontList font_list;
  for (std::u16string string : strings) {
    gfx::Size size = GetStringSize(string, font_list);
    EXPECT_EQ(GetStringWidth(string, font_list), size.width())
        << " input string is \"" << string << "\"";
    EXPECT_EQ(font_list.GetHeight(), size.height())
        << " input string is \"" << string << "\"";
  }
}

TEST(TextUtilsTest, AdjustVisualBorderForFont_BorderLargerThanFont) {
  FontList font_list;

  // We will make some assumptions about the default font - specifically that it
  // has leading space and space for the descender.
  DCHECK_GT(font_list.GetBaseline(), font_list.GetCapHeight());
  DCHECK_LT(font_list.GetBaseline(), font_list.GetHeight());

  // Adjust a large border for the default font. Using a large number means that
  // the border will extend outside the leading and descender area of the font.
  constexpr gfx::Insets kOriginalBorder(20);
  const gfx::Insets result =
      AdjustVisualBorderForFont(font_list, kOriginalBorder);
  EXPECT_EQ(result.left(), kOriginalBorder.left());
  EXPECT_EQ(result.right(), kOriginalBorder.right());
  EXPECT_LT(result.top(), kOriginalBorder.top());
  EXPECT_LT(result.bottom(), kOriginalBorder.bottom());
}

TEST(TextUtilsTest, AdjustVisualBorderForFont_BorderSmallerThanFont) {
  FontList font_list;

  // We will make some assumptions about the default font - specifically that it
  // has leading space and space for the descender.
  DCHECK_GT(font_list.GetBaseline(), font_list.GetCapHeight());
  DCHECK_LT(font_list.GetBaseline(), font_list.GetHeight());

  // Adjust a border with a small vertical component. The vertical component
  // should go to zero because it overlaps the leading and descender areas of
  // the font.
  constexpr auto kSmallVerticalInsets = gfx::Insets::VH(1, 20);
  const gfx::Insets result =
      AdjustVisualBorderForFont(font_list, kSmallVerticalInsets);
  EXPECT_EQ(result.left(), kSmallVerticalInsets.left());
  EXPECT_EQ(result.right(), kSmallVerticalInsets.right());
  EXPECT_EQ(result.top(), 0);
  EXPECT_EQ(result.bottom(), 0);
}

TEST(TextUtilsTest, GetFontCapHeightCenterOffset_SecondFontIsSmaller) {
  FontList original_font;
  FontList smaller_font = original_font.DeriveWithSizeDelta(-3);
  DCHECK_LT(smaller_font.GetCapHeight(), original_font.GetCapHeight());
  EXPECT_GT(GetFontCapHeightCenterOffset(original_font, smaller_font), 0);
}

TEST(TextUtilsTest, GetFontCapHeightCenterOffset_SecondFontIsLarger) {
  FontList original_font;
  FontList larger_font = original_font.DeriveWithSizeDelta(3);
  DCHECK_GT(larger_font.GetCapHeight(), original_font.GetCapHeight());
  EXPECT_LT(GetFontCapHeightCenterOffset(original_font, larger_font), 0);
}

TEST(TextUtilsTest, GetFontCapHeightCenterOffset_SameSize) {
  FontList original_font;
  EXPECT_EQ(0, GetFontCapHeightCenterOffset(original_font, original_font));
}

struct RemoveAcceleratorCharData {
  const char* input;
  int accelerated_char_pos;
  int accelerated_char_span;
  const char* output_locate_and_strip;
  const char* output_full_strip;
  const char* name;
};

class RemoveAcceleratorCharTest
    : public testing::TestWithParam<RemoveAcceleratorCharData> {
 public:
  static const RemoveAcceleratorCharData kCases[];
};

const RemoveAcceleratorCharData RemoveAcceleratorCharTest::kCases[] = {
    {"", -1, 0, "", "", "EmptyString"},
    {"&", -1, 0, "", "", "AcceleratorCharOnly"},
    {"no accelerator", -1, 0, "no accelerator", "no accelerator",
     "NoAccelerator"},
    {"&one accelerator", 0, 1, "one accelerator", "one accelerator",
     "OneAccelerator_Start"},
    {"one &accelerator", 4, 1, "one accelerator", "one accelerator",
     "OneAccelerator_Middle"},
    {"one accelerator&", -1, 0, "one accelerator", "one accelerator",
     "OneAccelerator_End"},
    {"&two &accelerators", 4, 1, "two accelerators", "two accelerators",
     "TwoAccelerators_OneAtStart"},
    {"two &accelerators&", 4, 1, "two accelerators", "two accelerators",
     "TwoAccelerators_OneAtEnd"},
    {"two& &accelerators", 4, 1, "two accelerators", "two accelerators",
     "TwoAccelerators_SpaceBetween"},
    {"&&escaping", -1, 0, "&escaping", "&escaping", "Escape_Start"},
    {"escap&&ing", -1, 0, "escap&ing", "escap&ing", "Escape_Middle"},
    {"escaping&&", -1, 0, "escaping&", "escaping&", "Escape_End"},
    {"accelerator(&A)", 12, 1, "accelerator(A)", "accelerator", "CJK_Style"},
    {"accelerator(&A)...", 12, 1, "accelerator(A)...", "accelerator...",
     "CJK_StyleEllipsis"},
    {"accelerator(paren", -1, 0, "accelerator(paren", "accelerator(paren",
     "CJK_OpenParen"},
    {"accelerator(&paren", 12, 1, "accelerator(paren", "accelerator(paren",
     "CJK_NoClosingParen"},
    {"accelerator(&paren)", 12, 1, "accelerator(paren)", "accelerator(paren)",
     "CJK_LateClosingParen"},
    {"accelerator&paren)", 11, 1, "acceleratorparen)", "acceleratorparen)",
     "CJK_NoOpeningParen"},
    {"accelerator(P)", -1, 0, "accelerator(P)", "accelerator(P)",
     "CJK_JustParens"},
    {"accelerator(&)", 12, 1, "accelerator()", "accelerator()",
     "CJK_MissingAccelerator"},
    {"&mix&&ed", 0, 1, "mix&ed", "mix&ed", "Mixed_EscapeAfterAccelerator"},
    {"&&m&ix&&e&d&", 6, 1, "&mix&ed", "&mix&ed",
     "Mixed_MiddleAcceleratorSkipped"},
    {"&&m&&ix&ed&&", 5, 1, "&m&ixed&", "&m&ixed&", "Mixed_OneAccelerator"},
    {"&m&&ix&ed&&", 4, 1, "m&ixed&", "m&ixed&",
     "Mixed_InitialAcceleratorSkipped"},
    // U+1D49C MATHEMATICAL SCRIPT CAPITAL A, which occupies two |char16|'s.
    {"&\U0001D49C", 0, 2, "\U0001D49C", "\U0001D49C",
     "MultibyteAccelerator_Start"},
    {"Test&\U0001D49Cing", 4, 2, "Test\U0001D49Cing", "Test\U0001D49Cing",
     "MultibyteAccelerator_Middle"},
    {"Test\U0001D49C&ing", 6, 1, "Test\U0001D49Cing", "Test\U0001D49Cing",
     "OneAccelerator_AfterMultibyte"},
    {"Test&\U0001D49C&ing", 6, 1, "Test\U0001D49Cing", "Test\U0001D49Cing",
     "MultibyteAccelerator_Skipped"},
    {"Test&\U0001D49C&&ing", 4, 2, "Test\U0001D49C&ing", "Test\U0001D49C&ing",
     "MultibyteAccelerator_EscapeAfter"},
    {"Test&\U0001D49C&\U0001D49Cing", 6, 2, "Test\U0001D49C\U0001D49Cing",
     "Test\U0001D49C\U0001D49Cing",
     "MultibyteAccelerator_AfterMultibyteAccelerator"},
    {"accelerator(&\U0001D49C)", 12, 2, "accelerator(\U0001D49C)",
     "accelerator", "MultibyteAccelerator_CJK"},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    RemoveAcceleratorCharTest,
    testing::ValuesIn(RemoveAcceleratorCharTest::kCases),
    [](const testing::TestParamInfo<RemoveAcceleratorCharData>& param_info) {
      return param_info.param.name;
    });

TEST_P(RemoveAcceleratorCharTest, RemoveAcceleratorChar) {
  RemoveAcceleratorCharData data = GetParam();
  int accelerated_char_pos;
  int accelerated_char_span;
  std::u16string result_locate_and_strip = LocateAndRemoveAcceleratorChar(
      base::UTF8ToUTF16(data.input), &accelerated_char_pos,
      &accelerated_char_span);
  std::u16string result_full_strip =
      RemoveAccelerator(base::UTF8ToUTF16(data.input));
  EXPECT_EQ(result_locate_and_strip,
            base::UTF8ToUTF16(data.output_locate_and_strip));
  EXPECT_EQ(result_full_strip, base::UTF8ToUTF16(data.output_full_strip));
  EXPECT_EQ(accelerated_char_pos, data.accelerated_char_pos);
  EXPECT_EQ(accelerated_char_span, data.accelerated_char_span);
}

struct FindValidBoundaryData {
  const char16_t* input;
  size_t index_in;
  bool trim_whitespace;
  size_t index_out;
  const char* name;
};

class FindValidBoundaryBeforeTest
    : public testing::TestWithParam<FindValidBoundaryData> {
 public:
  static const FindValidBoundaryData kCases[];
};

const FindValidBoundaryData FindValidBoundaryBeforeTest::kCases[] = {
    {u"", 0, false, 0, "Empty"},
    {u"word", 0, false, 0, "StartOfString"},
    {u"word", 4, false, 4, "EndOfString"},
    {u"word", 2, false, 2, "MiddleOfString_OnValidCharacter"},
    {u"w𐐷d", 2, false, 1, "MiddleOfString_OnSurrogatePair"},
    {u"w𐐷d", 1, false, 1, "MiddleOfString_BeforeSurrogatePair"},
    {u"w𐐷d", 3, false, 3, "MiddleOfString_AfterSurrogatePair"},
    {u"wo d", 3, false, 3, "MiddleOfString_OnSpace_NoTrim"},
    {u"wo d", 3, true, 2, "MiddleOfString_OnSpace_Trim"},
    {u"wo d", 2, false, 2, "MiddleOfString_LeftOfSpace_NoTrim"},
    {u"wo d", 2, true, 2, "MiddleOfString_LeftOfSpace_Trim"},
    {u"wo\td", 3, false, 3, "MiddleOfString_OnTab_NoTrim"},
    {u"wo\td", 3, true, 2, "MiddleOfString_OnTab_Trim"},
    {u"w  d", 3, false, 3, "MiddleOfString_MultipleWhitespace_NoTrim"},
    {u"w  d", 3, true, 1, "MiddleOfString_MultipleWhitespace_Trim"},
    {u"w  d", 2, false, 2, "MiddleOfString_MiddleOfWhitespace_NoTrim"},
    {u"w  d", 2, true, 1, "MiddleOfString_MiddleOfWhitespace_Trim"},
    {u"w  ", 3, false, 3, "EndOfString_Whitespace_NoTrim"},
    {u"w  ", 3, true, 1, "EndOfString_Whitespace_Trim"},
    {u"  d", 2, false, 2, "MiddleOfString_Whitespace_NoTrim"},
    {u"  d", 2, true, 0, "MiddleOfString_Whitespace_Trim"},
    // COMBINING GRAVE ACCENT (U+0300)
    {u"wo\u0300d", 2, false, 1, "MiddleOfString_OnCombiningMark"},
    {u"wo\u0300d", 1, false, 1, "MiddleOfString_BeforeCombiningMark"},
    {u"wo\u0300d", 3, false, 3, "MiddleOfString_AfterCombiningMark"},
    {u"w o\u0300d", 3, true, 1, "MiddleOfString_SpaceAndCombinginMark_Trim"},
    {u"wo\u0300 d", 3, true, 3, "MiddleOfString_CombiningMarkAndSpace_Trim"},
    {u"w \u0300d", 3, true, 3,
     "MiddleOfString_AfterSpaceWithCombiningMark_Trim"},
    {u"w \u0300d", 2, true, 1, "MiddleOfString_OnSpaceWithCombiningMark_Trim"},
    {u"w \u0300 d", 4, true, 3,
     "MiddleOfString_AfterSpaceAfterSpaceWithCombiningMark_Trim"},
    // MUSICAL SYMBOL G CLEF (U+1D11E) + MUSICAL SYMBOL COMBINING FLAG-1
    // (U+1D16E)
    {u"w\U0001D11E\U0001D16Ed", 1, false, 1,
     "MiddleOfString_BeforeCombiningSurrogate"},
    {u"w\U0001D11E\U0001D16Ed", 2, false, 1,
     "MiddleOfString_OnCombiningSurrogate_Pos1"},
    {u"w\U0001D11E\U0001D16Ed", 3, false, 1,
     "MiddleOfString_OnCombiningSurrogate_Pos2"},
    {u"w\U0001D11E\U0001D16Ed", 4, false, 1,
     "MiddleOfString_OnCombiningSurrogate_Pos3"},
    {u"w\U0001D11E\U0001D16Ed", 5, false, 5,
     "MiddleOfString_AfterCombiningSurrogate"},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FindValidBoundaryBeforeTest,
    testing::ValuesIn(FindValidBoundaryBeforeTest::kCases),
    [](const testing::TestParamInfo<FindValidBoundaryData>& param_info) {
      return param_info.param.name;
    });

TEST_P(FindValidBoundaryBeforeTest, FindValidBoundaryBefore) {
  FindValidBoundaryData data = GetParam();
  const std::u16string::const_pointer input =
      reinterpret_cast<std::u16string::const_pointer>(data.input);
  DLOG(INFO) << input;
  size_t result =
      FindValidBoundaryBefore(input, data.index_in, data.trim_whitespace);
  EXPECT_EQ(data.index_out, result);
}

class FindValidBoundaryAfterTest
    : public testing::TestWithParam<FindValidBoundaryData> {
 public:
  static const FindValidBoundaryData kCases[];
};

const FindValidBoundaryData FindValidBoundaryAfterTest::kCases[] = {
    {u"", 0, false, 0, "Empty"},
    {u"word", 0, false, 0, "StartOfString"},
    {u"word", 4, false, 4, "EndOfString"},
    {u"word", 2, false, 2, "MiddleOfString_OnValidCharacter"},
    {u"w𐐷d", 2, false, 3, "MiddleOfString_OnSurrogatePair"},
    {u"w𐐷d", 1, false, 1, "MiddleOfString_BeforeSurrogatePair"},
    {u"w𐐷d", 3, false, 3, "MiddleOfString_AfterSurrogatePair"},
    {u"wo d", 2, false, 2, "MiddleOfString_OnSpace_NoTrim"},
    {u"wo d", 2, true, 3, "MiddleOfString_OnSpace_Trim"},
    {u"wo d", 3, false, 3, "MiddleOfString_RightOfSpace_NoTrim"},
    {u"wo d", 3, true, 3, "MiddleOfString_RightOfSpace_Trim"},
    {u"wo\td", 2, false, 2, "MiddleOfString_OnTab_NoTrim"},
    {u"wo\td", 2, true, 3, "MiddleOfString_OnTab_Trim"},
    {u"w  d", 1, false, 1, "MiddleOfString_MultipleWhitespace_NoTrim"},
    {u"w  d", 1, true, 3, "MiddleOfString_MultipleWhitespace_Trim"},
    {u"w  d", 2, false, 2, "MiddleOfString_MiddleOfWhitespace_NoTrim"},
    {u"w  d", 2, true, 3, "MiddleOfString_MiddleOfWhitespace_Trim"},
    {u"w  ", 1, false, 1, "MiddleOfString_Whitespace_NoTrim"},
    {u"w  ", 1, true, 3, "MiddleOfString_Whitespace_Trim"},
    {u"  d", 0, false, 0, "StartOfString_Whitespace_NoTrim"},
    {u"  d", 0, true, 2, "StartOfString_Whitespace_Trim"},
    // COMBINING GRAVE ACCENT (U+0300)
    {u"wo\u0300d", 2, false, 3, "MiddleOfString_OnCombiningMark"},
    {u"wo\u0300d", 1, false, 1, "MiddleOfString_BeforeCombiningMark"},
    {u"wo\u0300d", 3, false, 3, "MiddleOfString_AfterCombiningMark"},
    {u"w o\u0300d", 1, true, 2, "MiddleOfString_SpaceAndCombinginMark_Trim"},
    {u"wo\u0300 d", 1, true, 1,
     "MiddleOfString_BeforeCombiningMarkAndSpace_Trim"},
    {u"wo\u0300 d", 2, true, 4, "MiddleOfString_OnCombiningMarkAndSpace_Trim"},
    {u"w \u0300d", 1, true, 1,
     "MiddleOfString_BeforeSpaceWithCombiningMark_Trim"},
    {u"w \u0300d", 2, true, 3, "MiddleOfString_OnSpaceWithCombiningMark_Trim"},
    {u"w  \u0300d", 1, true, 2,
     "MiddleOfString_BeforeSpaceBeforeSpaceWithCombiningMark_Trim"},
    // MUSICAL SYMBOL G CLEF (U+1D11E) + MUSICAL SYMBOL COMBINING FLAG-1
    // (U+1D16E)
    {u"w\U0001D11E\U0001D16Ed", 1, false, 1,
     "MiddleOfString_BeforeCombiningSurrogate"},
    {u"w\U0001D11E\U0001D16Ed", 2, false, 5,
     "MiddleOfString_OnCombiningSurrogate_Pos1"},
    {u"w\U0001D11E\U0001D16Ed", 3, false, 5,
     "MiddleOfString_OnCombiningSurrogate_Pos2"},
    {u"w\U0001D11E\U0001D16Ed", 4, false, 5,
     "MiddleOfString_OnCombiningSurrogate_Pos3"},
    {u"w\U0001D11E\U0001D16Ed", 5, false, 5,
     "MiddleOfString_AfterCombiningSurrogate"},
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FindValidBoundaryAfterTest,
    testing::ValuesIn(FindValidBoundaryAfterTest::kCases),
    [](const testing::TestParamInfo<FindValidBoundaryData>& param_info) {
      return param_info.param.name;
    });

TEST_P(FindValidBoundaryAfterTest, FindValidBoundaryAfter) {
  FindValidBoundaryData data = GetParam();
  const std::u16string::const_pointer input =
      reinterpret_cast<std::u16string::const_pointer>(data.input);
  size_t result =
      FindValidBoundaryAfter(input, data.index_in, data.trim_whitespace);
  EXPECT_EQ(data.index_out, result);
}

}  // namespace
}  // namespace gfx
