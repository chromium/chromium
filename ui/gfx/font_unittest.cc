// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font_names_testing.h"

#if defined(OS_WIN)
#include "ui/gfx/system_fonts_win.h"
#endif

namespace gfx {
namespace {

class FontTest : public testing::Test {
 public:
  FontTest() = default;

 protected:
  void SetUp() override {
#if defined(OS_WIN)
    // System fonts is keeping a cache of loaded system fonts. These fonts are
    // scaled based on global callbacks configured on startup. The tests in this
    // file are testing these callbacks and need to be sure we cleared the
    // global state to avoid flaky tests.
    win::ResetSystemFontsForTesting();
#endif
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FontTest);
};

TEST_F(FontTest, DefaultFont) {
  Font cf;
  EXPECT_EQ(cf.GetStyle(), Font::NORMAL);
  EXPECT_EQ(cf.GetWeight(), Font::Weight::NORMAL);
  // Ensures that font metrics are generated. Some fonts backends do not provide
  // some metrics (e.g. DWrite do not produce average character width).
  EXPECT_GT(cf.GetFontSize(), 0);
  EXPECT_GT(cf.GetHeight(), 0);
  EXPECT_GT(cf.GetBaseline(), 0);
  EXPECT_GT(cf.GetCapHeight(), 0);
  EXPECT_GT(cf.GetExpectedTextWidth(1), 0);
}

TEST_F(FontTest, LoadArial) {
  Font cf(kTestFontName, 16);
#if defined(OS_MACOSX) || defined(OS_IOS)
  EXPECT_TRUE(cf.GetNativeFont());
#endif
  EXPECT_EQ(cf.GetStyle(), Font::NORMAL);
  EXPECT_EQ(cf.GetFontSize(), 16);
  EXPECT_EQ(cf.GetFontName(), kTestFontName);
  EXPECT_EQ(base::ToLowerASCII(kTestFontName),
            base::ToLowerASCII(cf.GetActualFontName()));
}

TEST_F(FontTest, LoadArialBold) {
  Font cf(kTestFontName, 16);
  Font bold(cf.Derive(0, Font::NORMAL, Font::Weight::BOLD));
#if defined(OS_MACOSX) || defined(OS_IOS)
  EXPECT_TRUE(bold.GetNativeFont());
#endif
  EXPECT_EQ(bold.GetStyle(), Font::NORMAL);
  EXPECT_EQ(bold.GetWeight(), Font::Weight::BOLD);
  EXPECT_EQ(base::ToLowerASCII(kTestFontName),
            base::ToLowerASCII(cf.GetActualFontName()));
}

TEST_F(FontTest, Ascent) {
  Font cf(kTestFontName, 16);
  EXPECT_GT(cf.GetBaseline(), 2);
  EXPECT_LE(cf.GetBaseline(), 22);
}

TEST_F(FontTest, Height) {
  Font cf(kTestFontName, 16);
  EXPECT_GE(cf.GetHeight(), 16);
  // TODO(akalin): Figure out why height is so large on Linux.
  EXPECT_LE(cf.GetHeight(), 26);
}

TEST_F(FontTest, CapHeight) {
  Font cf(kTestFontName, 16);
  EXPECT_GT(cf.GetCapHeight(), 0);
  EXPECT_GT(cf.GetCapHeight(), cf.GetHeight() / 2);
  EXPECT_LT(cf.GetCapHeight(), cf.GetBaseline());
}

TEST_F(FontTest, AvgWidths) {
  Font cf(kTestFontName, 16);
  EXPECT_EQ(cf.GetExpectedTextWidth(0), 0);
  EXPECT_GT(cf.GetExpectedTextWidth(1), cf.GetExpectedTextWidth(0));
  EXPECT_GT(cf.GetExpectedTextWidth(2), cf.GetExpectedTextWidth(1));
  EXPECT_GT(cf.GetExpectedTextWidth(3), cf.GetExpectedTextWidth(2));
}

// Check that fonts used for testing are installed and enabled. On Mac
// fonts may be installed but still need enabling in Font Book.app.
// http://crbug.com/347429
TEST_F(FontTest, GetActualFontName) {
  Font arial(kTestFontName, 16);
  EXPECT_EQ(base::ToLowerASCII(kTestFontName),
            base::ToLowerASCII(arial.GetActualFontName()))
      << "********\n"
      << "Your test environment seems to be missing Arial font, which is "
      << "needed for unittests.  Check if Arial font is installed.\n"
      << "********";
  Font symbol(kSymbolFontName, 16);
  EXPECT_EQ(base::ToLowerASCII(kSymbolFontName),
            base::ToLowerASCII(symbol.GetActualFontName()))
      << "********\n"
      << "Your test environment seems to be missing the " << kSymbolFontName
      << " font, which is "
      << "needed for unittests.  Check if " << kSymbolFontName
      << " font is installed.\n"
      << "********";

  const char* const invalid_font_name = "no_such_font_name";
  Font fallback_font(invalid_font_name, 16);
  EXPECT_NE(invalid_font_name,
            base::ToLowerASCII(fallback_font.GetActualFontName()));
}

TEST_F(FontTest, DeriveFont) {
  Font cf(kTestFontName, 8);
  const int kSizeDelta = 2;
  Font cf_underlined =
      cf.Derive(0, cf.GetStyle() | gfx::Font::UNDERLINE, cf.GetWeight());
  Font cf_underlined_resized = cf_underlined.Derive(
      kSizeDelta, cf_underlined.GetStyle(), cf_underlined.GetWeight());
  EXPECT_EQ(cf.GetStyle() | gfx::Font::UNDERLINE,
            cf_underlined_resized.GetStyle());
  EXPECT_EQ(cf.GetFontSize() + kSizeDelta, cf_underlined_resized.GetFontSize());
  EXPECT_EQ(cf.GetWeight(), cf_underlined_resized.GetWeight());
}

#if defined(OS_WIN)
TEST_F(FontTest, DeriveResizesIfSizeTooSmall) {
  Font cf(kTestFontName, 8);
  gfx::win::SetGetMinimumFontSizeCallback([] { return 5; });

  Font derived_font = cf.Derive(-4, cf.GetStyle(), cf.GetWeight());
  EXPECT_EQ(5, derived_font.GetFontSize());
}

TEST_F(FontTest, DeriveKeepsOriginalSizeIfHeightOk) {
  Font cf(kTestFontName, 8);
  gfx::win::SetGetMinimumFontSizeCallback([] { return 5; });

  Font derived_font = cf.Derive(-2, cf.GetStyle(), cf.GetWeight());
  EXPECT_EQ(6, derived_font.GetFontSize());
}
#endif  // defined(OS_WIN)

TEST_F(FontTest, WeightConversion) {
  struct WeightMatchExpectation {
    int weight;
    Font::Weight enum_value;
  } expectations[] = {
      {-10, Font::Weight::INVALID}, {-1, Font::Weight::INVALID},
      {0, Font::Weight::THIN},      {1, Font::Weight::THIN},
      {100, Font::Weight::THIN},    {350, Font::Weight::NORMAL},
      {400, Font::Weight::NORMAL},  {899, Font::Weight::BLACK},
      {900, Font::Weight::BLACK},   {901, Font::Weight::INVALID}};
  for (const auto& expectation : expectations) {
    EXPECT_EQ(FontWeightFromInt(expectation.weight), expectation.enum_value);
  }
}

}  // namespace
}  // namespace gfx
