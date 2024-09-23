// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/utf_offset.h"

#include <optional>
#include <string>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

TEST(UtfOffsetTest, Utf16OffsetFromUtf8Offset) {
  constexpr struct {
    const char* str;
    size_t offset;
    std::optional<size_t> expect;
  } kTestCases[] = {
      // 1 byte letters.
      {"ab", 0, 0},
      {"ab", 1, 1},
      {"ab", 2, 2},
      {"ab", 3, std::nullopt},

      // 2 byte letters. \u03A9=\xCE\xA9 is greek OMEGA.
      {"\u03A9\u03A9", 0, 0},
      {"\u03A9\u03A9", 1, std::nullopt},
      {"\u03A9\u03A9", 2, 1},
      {"\u03A9\u03A9", 3, std::nullopt},
      {"\u03A9\u03A9", 4, 2},
      {"\u03A9\u03A9", 5, std::nullopt},

      // 3 byte letters. \u3042=\xE3\x81\x82 is Japanese "A".
      {"\u3042\u3042", 0, 0},
      {"\u3042\u3042", 1, std::nullopt},
      {"\u3042\u3042", 2, std::nullopt},
      {"\u3042\u3042", 3, 1},
      {"\u3042\u3042", 4, std::nullopt},
      {"\u3042\u3042", 5, std::nullopt},
      {"\u3042\u3042", 6, 2},
      {"\u3042\u3042", 7, std::nullopt},

      // 4 byte letters. \U0001F3B7=\xF0\x9F\x8E\xB7 is "SAXOPHONE" emoji.
      // Note that a surrogate pair advances by 2 in UTF16.
      {"\U0001F3B7\U0001F3B7", 0, 0},
      {"\U0001F3B7\U0001F3B7", 1, std::nullopt},
      {"\U0001F3B7\U0001F3B7", 2, std::nullopt},
      {"\U0001F3B7\U0001F3B7", 3, std::nullopt},
      {"\U0001F3B7\U0001F3B7", 4, 2},
      {"\U0001F3B7\U0001F3B7", 5, std::nullopt},
      {"\U0001F3B7\U0001F3B7", 6, std::nullopt},
      {"\U0001F3B7\U0001F3B7", 7, std::nullopt},
      {"\U0001F3B7\U0001F3B7", 8, 4},
      {"\U0001F3B7\U0001F3B7", 9, std::nullopt},

      // Mix case.
      {"a\u03A9b\u3042c\U0001F3B7d", 0, 0},
      {"a\u03A9b\u3042c\U0001F3B7d", 1, 1},
      {"a\u03A9b\u3042c\U0001F3B7d", 2, std::nullopt},
      {"a\u03A9b\u3042c\U0001F3B7d", 3, 2},
      {"a\u03A9b\u3042c\U0001F3B7d", 4, 3},
      {"a\u03A9b\u3042c\U0001F3B7d", 5, std::nullopt},
      {"a\u03A9b\u3042c\U0001F3B7d", 6, std::nullopt},
      {"a\u03A9b\u3042c\U0001F3B7d", 7, 4},
      {"a\u03A9b\u3042c\U0001F3B7d", 8, 5},
      {"a\u03A9b\u3042c\U0001F3B7d", 9, std::nullopt},
      {"a\u03A9b\u3042c\U0001F3B7d", 10, std::nullopt},
      {"a\u03A9b\u3042c\U0001F3B7d", 11, std::nullopt},
      {"a\u03A9b\u3042c\U0001F3B7d", 12, 7},
      {"a\u03A9b\u3042c\U0001F3B7d", 13, 8},
      {"a\u03A9b\u3042c\U0001F3B7d", 14, std::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expect,
              Utf16OffsetFromUtf8Offset(test_case.str, test_case.offset))
        << " at " << test_case.str << "[" << test_case.offset << "]";
  }
}

TEST(UtfOffsetTest, Utf8OffsetFromUtf16Offset) {
  constexpr struct {
    const char16_t* str;
    size_t offset;
    std::optional<size_t> expect;
  } kTestCases[] = {
      // 1 byte letters.
      {u"ab", 0, 0},
      {u"ab", 1, 1},
      {u"ab", 2, 2},
      {u"ab", 3, std::nullopt},

      // 2 byte letters.
      {u"\u03A9\u03A9", 0, 0},
      {u"\u03A9\u03A9", 1, 2},
      {u"\u03A9\u03A9", 2, 4},
      {u"\u03A9\u03A9", 3, std::nullopt},

      // 3 byte letters.
      {u"\u3042\u3042", 0, 0},
      {u"\u3042\u3042", 1, 3},
      {u"\u3042\u3042", 2, 6},
      {u"\u3042\u3042", 3, std::nullopt},

      // 4 byte letters = surrogate pairs.
      {u"\U0001F3B7\U0001F3B7", 0, 0},
      {u"\U0001F3B7\U0001F3B7", 1, std::nullopt},
      {u"\U0001F3B7\U0001F3B7", 2, 4},
      {u"\U0001F3B7\U0001F3B7", 3, std::nullopt},
      {u"\U0001F3B7\U0001F3B7", 4, 8},
      {u"\U0001F3B7\U0001F3B7", 5, std::nullopt},
      {u"\U0001F3B7\U0001F3B7", 6, std::nullopt},

      // Mix case.
      {u"a\u03A9b\u3042c\U0001F3B7d", 0, 0},
      {u"a\u03A9b\u3042c\U0001F3B7d", 1, 1},
      {u"a\u03A9b\u3042c\U0001F3B7d", 2, 3},
      {u"a\u03A9b\u3042c\U0001F3B7d", 3, 4},
      {u"a\u03A9b\u3042c\U0001F3B7d", 4, 7},
      {u"a\u03A9b\u3042c\U0001F3B7d", 5, 8},
      {u"a\u03A9b\u3042c\U0001F3B7d", 6, std::nullopt},
      {u"a\u03A9b\u3042c\U0001F3B7d", 7, 12},
      {u"a\u03A9b\u3042c\U0001F3B7d", 8, 13},
      {u"a\u03A9b\u3042c\U0001F3B7d", 9, std::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.expect,
              Utf8OffsetFromUtf16Offset(test_case.str, test_case.offset))
        << " at " << test_case.str << "[" << test_case.offset << "]";
  }
}

}  // namespace
}  // namespace ui
