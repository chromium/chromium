// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/font_fallback_linux.h"

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_fallback.h"

namespace gfx {

namespace {
const char kDefaultApplicationLocale[] = "us-en";
const char kFrenchApplicationLocale[] = "ca-fr";
}  // namespace

class FontFallbackLinuxTest : public testing::Test {
 public:
  void SetUp() override {
    // Clear the font fallback caches.
    ClearAllFontFallbackCachesForTesting();
  }
};

// If the Type 1 Symbol.pfb font is installed, it is returned as fallback font
// for the PUA character 0xf6db. This test ensures we're not returning Type 1
// fonts as fallback.
TEST_F(FontFallbackLinuxTest, NoType1InFallbackFonts) {
  FallbackFontData font_fallback_data;
  if (GetFallbackFontForChar(0xf6db, std::string(), &font_fallback_data)) {
    std::string extension = font_fallback_data.filepath.Extension();
    if (!extension.empty())
      EXPECT_NE(extension, ".pfb");
  }
}

TEST_F(FontFallbackLinuxTest, GetFallbackFont) {
  Font base_font;

  Font fallback_font_cjk;
  EXPECT_TRUE(GetFallbackFont(base_font, kDefaultApplicationLocale, u"⻩",
                              &fallback_font_cjk));
  EXPECT_EQ(fallback_font_cjk.GetFontName(), "Noto Sans CJK JP");

  Font fallback_font_khmer;
  EXPECT_TRUE(GetFallbackFont(base_font, kDefaultApplicationLocale, u"ឨឮឡ",
                              &fallback_font_khmer));
  EXPECT_EQ(fallback_font_khmer.GetFontName(), "Noto Sans Khmer");
}

TEST_F(FontFallbackLinuxTest, GetFallbackFontCache) {
  EXPECT_EQ(0U, GetFallbackFontEntriesCacheSizeForTesting());

  Font base_font;
  Font fallback_font;
  EXPECT_TRUE(GetFallbackFont(base_font, kDefaultApplicationLocale, u"⻩",
                              &fallback_font));
  EXPECT_EQ(1U, GetFallbackFontEntriesCacheSizeForTesting());

  // Second call should not increase the cache size.
  EXPECT_TRUE(GetFallbackFont(base_font, kDefaultApplicationLocale, u"⻩",
                              &fallback_font));
  EXPECT_EQ(1U, GetFallbackFontEntriesCacheSizeForTesting());

  // Third call with a different code point in the same font, should not
  // increase the cache size.
  EXPECT_TRUE(GetFallbackFont(base_font, kDefaultApplicationLocale, u"⻪",
                              &fallback_font));
  EXPECT_EQ(1U, GetFallbackFontEntriesCacheSizeForTesting());

  // A different locale should trigger an new cache entry.
  EXPECT_TRUE(GetFallbackFont(base_font, kFrenchApplicationLocale, u"⻩",
                              &fallback_font));
  EXPECT_EQ(2U, GetFallbackFontEntriesCacheSizeForTesting());

  // The fallbackfonts cache should not be affected.
  EXPECT_EQ(0U, GetFallbackFontListCacheSizeForTesting());
}

TEST_F(FontFallbackLinuxTest, Fallbacks) {
  EXPECT_EQ(0U, GetFallbackFontListCacheSizeForTesting());

  Font default_font("sans", 13);
  std::vector<Font> fallbacks = GetFallbackFonts(default_font);
  EXPECT_FALSE(fallbacks.empty());
  EXPECT_EQ(1U, GetFallbackFontListCacheSizeForTesting());

  // The first fallback should be 'DejaVu Sans' which is the default linux
  // fonts. The fonts on linux are mock with test_fonts (see
  // third_party/tests_font).
  if (!fallbacks.empty())
    EXPECT_EQ(fallbacks[0].GetFontName(), "DejaVu Sans");

  // Second lookup should not increase the cache size.
  fallbacks = GetFallbackFonts(default_font);
  EXPECT_FALSE(fallbacks.empty());
  EXPECT_EQ(1U, GetFallbackFontListCacheSizeForTesting());

  // The fallbackfont cache should not be affected.
  EXPECT_EQ(0U, GetFallbackFontEntriesCacheSizeForTesting());
}

}  // namespace gfx
