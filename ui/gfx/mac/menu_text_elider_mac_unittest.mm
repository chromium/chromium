// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/menu_text_elider_mac.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/text_elider.h"

namespace gfx {

TEST(MenuTextEliderMacTest, ShortTitleNotTruncated) {
  std::u16string short_title = u"Short Title";
  std::u16string result = ElideMenuItemTitle(short_title);
  EXPECT_EQ(short_title, result);
}

TEST(MenuTextEliderMacTest, LongTitleTruncatedWithMiddleEllipsis) {
  std::u16string long_title =
      u"This is a very long menu item title that should definitely be "
      u"truncated because it exceeds the maximum width allowed for menu items "
      u"in the system";
  std::u16string result = ElideMenuItemTitle(long_title);

  // The result should be shorter than the original.
  EXPECT_LT(result.length(), long_title.length());

  // The result should contain the ellipsis character.
  EXPECT_NE(result.find(kEllipsisUTF16), std::u16string::npos);

  // The result should start with the beginning of the original title.
  EXPECT_TRUE(result.substr(0, 5) == long_title.substr(0, 5));
}

TEST(MenuTextEliderMacTest, EmptyTitleReturnsEmpty) {
  std::u16string empty_title;
  std::u16string result = ElideMenuItemTitle(empty_title);
  EXPECT_TRUE(result.empty());
}

TEST(MenuTextEliderMacTest, CustomWidthRespected) {
  std::u16string title = u"Medium length title for testing";

  // Very small width should force truncation.
  std::u16string narrow_result = ElideMenuItemTitle(title, 50.0f);
  EXPECT_LT(narrow_result.length(), title.length());

  // Very large width should not truncate.
  std::u16string wide_result = ElideMenuItemTitle(title, 1000.0f);
  EXPECT_EQ(wide_result, title);
}

TEST(MenuTextEliderMacTest, ChineseTextTruncation) {
  std::u16string chinese_title = u"这是一个非常长的中文标题，用于测试菜单项标题"
                                 u"截断功能是否正确处理中文字符";
  std::u16string result = ElideMenuItemTitle(chinese_title, 100.0f);

  // Chinese text with narrow width should be truncated.
  EXPECT_LT(result.length(), chinese_title.length());
  EXPECT_NE(result.find(kEllipsisUTF16), std::u16string::npos);
}

TEST(MenuTextEliderMacTest, DefaultMaxWidthConstant) {
  // Verify the constant has the expected value.
  EXPECT_FLOAT_EQ(kDefaultMenuItemTitleMaxWidth, 400.0f);
}

TEST(MenuTextEliderMacTest, EmojiStringTruncationPreservesCompleteEmojis) {
  // A string composed entirely of emojis.
  std::u16string emoji_title =
      u"🎉🎊🎁🎂🎈🎀🎄🎃🎅🎆🎇🎐🎑🎒🎓🎠🎡🎢🎣🎤🎥🎦🎧🎨🎩🎪🎫🎬🎭🎮🎯";
  std::u16string result = ElideMenuItemTitle(emoji_title, 150.0f);

  // The result should be truncated (shorter than original).
  EXPECT_LT(result.length(), emoji_title.length());

  // The result should contain the ellipsis.
  EXPECT_NE(result.find(kEllipsisUTF16), std::u16string::npos);

  // Verify all characters except the ellipsis are complete emojis.
  // Remove the ellipsis and check that remaining characters are valid emojis.
  std::u16string without_ellipsis;
  for (size_t i = 0; i < result.length(); ++i) {
    // Skip the ellipsis character.
    if (result[i] == kEllipsisUTF16[0]) {
      continue;
    }
    without_ellipsis += result[i];
  }

  // Each emoji in the original string is a single code point (no surrogate
  // pairs for these specific emojis), so verify no broken surrogates exist.
  // A broken surrogate would be a high surrogate (0xD800-0xDBFF) not followed
  // by a low surrogate (0xDC00-0xDFFF), or a lone low surrogate.
  for (size_t i = 0; i < without_ellipsis.length(); ++i) {
    char16_t c = without_ellipsis[i];
    if (c >= 0xD800 && c <= 0xDBFF) {
      // High surrogate - must be followed by low surrogate.
      ASSERT_LT(i + 1, without_ellipsis.length())
          << "Broken surrogate pair: high surrogate at end of string";
      char16_t next = without_ellipsis[i + 1];
      EXPECT_TRUE(next >= 0xDC00 && next <= 0xDFFF)
          << "Broken surrogate pair: high surrogate not followed by low "
             "surrogate";
      ++i;  // Skip the low surrogate.
    } else {
      // Should not be a lone low surrogate.
      EXPECT_FALSE(c >= 0xDC00 && c <= 0xDFFF)
          << "Broken surrogate pair: lone low surrogate";
    }
  }
}

}  // namespace gfx
