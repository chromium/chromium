// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/utf_offset.h"

#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {

TEST(UtfOffsetTest, Utf16OffsetFromUtf8Offset) {
  constexpr struct {
    const char* str;
    size_t offset;
    base::Optional<size_t> expect;
  } kTestCases[] = {
      // 1 byte letters.
      {u8"ab", 0, 0},
      {u8"ab", 1, 1},
      {u8"ab", 2, 2},
      {u8"ab", 3, base::nullopt},

      // 2 byte letters. \u03A9=\xCE\xA9 is greek OMEGA.
      {u8"\u03A9\u03A9", 0, 0},
      {u8"\u03A9\u03A9", 1, base::nullopt},
      {u8"\u03A9\u03A9", 2, 1},
      {u8"\u03A9\u03A9", 3, base::nullopt},
      {u8"\u03A9\u03A9", 4, 2},
      {u8"\u03A9\u03A9", 5, base::nullopt},

      // 3 byte letters. \u3042=\xE3\x81\x82 is Japanese "A".
      {u8"\u3042\u3042", 0, 0},
      {u8"\u3042\u3042", 1, base::nullopt},
      {u8"\u3042\u3042", 2, base::nullopt},
      {u8"\u3042\u3042", 3, 1},
      {u8"\u3042\u3042", 4, base::nullopt},
      {u8"\u3042\u3042", 5, base::nullopt},
      {u8"\u3042\u3042", 6, 2},
      {u8"\u3042\u3042", 7, base::nullopt},

      // 4 byte letters. \U0001F3B7=\xF0\x9F\x8E\xB7 is "SAXOPHONE" emoji.
      // Note that a surrogate pair advances by 2 in UTF16.
      {u8"\U0001F3B7\U0001F3B7", 0, 0},
      {u8"\U0001F3B7\U0001F3B7", 1, base::nullopt},
      {u8"\U0001F3B7\U0001F3B7", 2, base::nullopt},
      {u8"\U0001F3B7\U0001F3B7", 3, base::nullopt},
      {u8"\U0001F3B7\U0001F3B7", 4, 2},
      {u8"\U0001F3B7\U0001F3B7", 5, base::nullopt},
      {u8"\U0001F3B7\U0001F3B7", 6, base::nullopt},
      {u8"\U0001F3B7\U0001F3B7", 7, base::nullopt},
      {u8"\U0001F3B7\U0001F3B7", 8, 4},
      {u8"\U0001F3B7\U0001F3B7", 9, base::nullopt},

      // Mix case.
      {u8"a\u03A9b\u3042c\U0001F3B7d", 0, 0},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 1, 1},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 2, base::nullopt},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 3, 2},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 4, 3},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 5, base::nullopt},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 6, base::nullopt},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 7, 4},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 8, 5},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 9, base::nullopt},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 10, base::nullopt},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 11, base::nullopt},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 12, 7},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 13, 8},
      {u8"a\u03A9b\u3042c\U0001F3B7d", 14, base::nullopt},
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
    base::Optional<size_t> expect;
  } kTestCases[] = {
      // 1 byte letters.
      {u"ab", 0, 0},
      {u"ab", 1, 1},
      {u"ab", 2, 2},
      {u"ab", 3, base::nullopt},

      // 2 byte letters.
      {u"\u03A9\u03A9", 0, 0},
      {u"\u03A9\u03A9", 1, 2},
      {u"\u03A9\u03A9", 2, 4},
      {u"\u03A9\u03A9", 3, base::nullopt},

      // 3 byte letters.
      {u"\u3042\u3042", 0, 0},
      {u"\u3042\u3042", 1, 3},
      {u"\u3042\u3042", 2, 6},
      {u"\u3042\u3042", 3, base::nullopt},

      // 4 byte letters = surrogate pairs.
      {u"\U0001F3B7\U0001F3B7", 0, 0},
      {u"\U0001F3B7\U0001F3B7", 1, base::nullopt},
      {u"\U0001F3B7\U0001F3B7", 2, 4},
      {u"\U0001F3B7\U0001F3B7", 3, base::nullopt},
      {u"\U0001F3B7\U0001F3B7", 4, 8},
      {u"\U0001F3B7\U0001F3B7", 5, base::nullopt},
      {u"\U0001F3B7\U0001F3B7", 6, base::nullopt},

      // Mix case.
      {u"a\u03A9b\u3042c\U0001F3B7d", 0, 0},
      {u"a\u03A9b\u3042c\U0001F3B7d", 1, 1},
      {u"a\u03A9b\u3042c\U0001F3B7d", 2, 3},
      {u"a\u03A9b\u3042c\U0001F3B7d", 3, 4},
      {u"a\u03A9b\u3042c\U0001F3B7d", 4, 7},
      {u"a\u03A9b\u3042c\U0001F3B7d", 5, 8},
      {u"a\u03A9b\u3042c\U0001F3B7d", 6, base::nullopt},
      {u"a\u03A9b\u3042c\U0001F3B7d", 7, 12},
      {u"a\u03A9b\u3042c\U0001F3B7d", 8, 13},
      {u"a\u03A9b\u3042c\U0001F3B7d", 9, base::nullopt},
  };

  for (const auto& test_case : kTestCases) {
    // TODO(crbug.com/911896): Get rid of reinterpret_cast on switching
    // to char16_t.
    base::string16 text(reinterpret_cast<const base::char16*>(test_case.str));
    EXPECT_EQ(test_case.expect,
              Utf8OffsetFromUtf16Offset(text, test_case.offset))
        << " at " << text << "[" << test_case.offset << "]";
  }
}

}  // namespace
}  // namespace ui
