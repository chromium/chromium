// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/bidi_line_iterator.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace gfx {
namespace {

class BiDiLineIteratorTest
    : public testing::TestWithParam<base::i18n::TextDirection> {
 public:
  BiDiLineIteratorTest() = default;

  BiDiLineIteratorTest(const BiDiLineIteratorTest&) = delete;
  BiDiLineIteratorTest& operator=(const BiDiLineIteratorTest&) = delete;

  BiDiLineIterator* iterator() { return &iterator_; }

 private:
  BiDiLineIterator iterator_;
};

TEST_P(BiDiLineIteratorTest, OnlyLTR) {
  iterator()->Open(u"abc 😁 测试", GetParam());
  ASSERT_EQ(1, iterator()->CountRuns());

  int start, length;
  EXPECT_EQ(UBIDI_LTR, iterator()->GetVisualRun(0, &start, &length));
  EXPECT_EQ(0, start);
  EXPECT_EQ(9, length);

  int end;
  UBiDiLevel level;
  iterator()->GetLogicalRun(0, &end, &level);
  EXPECT_EQ(9, end);
  if (GetParam() == base::i18n::TextDirection::RIGHT_TO_LEFT)
    EXPECT_EQ(2, level);
  else
    EXPECT_EQ(0, level);
}

TEST_P(BiDiLineIteratorTest, OnlyRTL) {
  iterator()->Open(u"מה השעה", GetParam());
  ASSERT_EQ(1, iterator()->CountRuns());

  int start, length;
  EXPECT_EQ(UBIDI_RTL, iterator()->GetVisualRun(0, &start, &length));
  EXPECT_EQ(0, start);
  EXPECT_EQ(7, length);

  int end;
  UBiDiLevel level;
  iterator()->GetLogicalRun(0, &end, &level);
  EXPECT_EQ(7, end);
  EXPECT_EQ(1, level);
}

TEST_P(BiDiLineIteratorTest, Mixed) {
  iterator()->Open(u"אני משתמש ב- Chrome כדפדפן האינטרנט שלי", GetParam());
  ASSERT_EQ(3, iterator()->CountRuns());

  // We'll get completely different results depending on the top-level paragraph
  // direction.
  if (GetParam() == base::i18n::TextDirection::RIGHT_TO_LEFT) {
    // If para direction is RTL, expect the LTR substring "Chrome" to be nested
    // within the surrounding RTL text.
    int start, length;
    EXPECT_EQ(UBIDI_RTL, iterator()->GetVisualRun(0, &start, &length));
    EXPECT_EQ(19, start);
    EXPECT_EQ(20, length);
    EXPECT_EQ(UBIDI_LTR, iterator()->GetVisualRun(1, &start, &length));
    EXPECT_EQ(13, start);
    EXPECT_EQ(6, length);
    EXPECT_EQ(UBIDI_RTL, iterator()->GetVisualRun(2, &start, &length));
    EXPECT_EQ(0, start);
    EXPECT_EQ(13, length);

    int end;
    UBiDiLevel level;
    iterator()->GetLogicalRun(0, &end, &level);
    EXPECT_EQ(13, end);
    EXPECT_EQ(1, level);
    iterator()->GetLogicalRun(13, &end, &level);
    EXPECT_EQ(19, end);
    EXPECT_EQ(2, level);
    iterator()->GetLogicalRun(19, &end, &level);
    EXPECT_EQ(39, end);
    EXPECT_EQ(1, level);
  } else {
    // If the para direction is LTR, expect the LTR substring "- Chrome " to be
    // at the top level, with two nested RTL runs on either side.
    int start, length;
    EXPECT_EQ(UBIDI_RTL, iterator()->GetVisualRun(0, &start, &length));
    EXPECT_EQ(0, start);
    EXPECT_EQ(11, length);
    EXPECT_EQ(UBIDI_LTR, iterator()->GetVisualRun(1, &start, &length));
    EXPECT_EQ(11, start);
    EXPECT_EQ(9, length);
    EXPECT_EQ(UBIDI_RTL, iterator()->GetVisualRun(2, &start, &length));
    EXPECT_EQ(20, start);
    EXPECT_EQ(19, length);

    int end;
    UBiDiLevel level;
    iterator()->GetLogicalRun(0, &end, &level);
    EXPECT_EQ(11, end);
    EXPECT_EQ(1, level);
    iterator()->GetLogicalRun(11, &end, &level);
    EXPECT_EQ(20, end);
    EXPECT_EQ(0, level);
    iterator()->GetLogicalRun(20, &end, &level);
    EXPECT_EQ(39, end);
    EXPECT_EQ(1, level);
  }
}

TEST_P(BiDiLineIteratorTest, RTLPunctuationNoCustomBehavior) {
  // This string features Hebrew characters interleaved with ASCII punctuation.
  iterator()->Open(
      u"א!ב\"ג#ד$ה%ו&ז'ח(ט)י*ך+כ,ל-ם.מ/"
      u"ן:נ;ס<ע=ף>פ?ץ@צ[ק\\ר]ש^ת_א`ב{ג|ד}ה~ו",
      GetParam());

  // Expect a single RTL run.
  ASSERT_EQ(1, iterator()->CountRuns());

  int start, length;
  EXPECT_EQ(UBIDI_RTL, iterator()->GetVisualRun(0, &start, &length));
  EXPECT_EQ(0, start);
  EXPECT_EQ(65, length);

  int end;
  UBiDiLevel level;
  iterator()->GetLogicalRun(0, &end, &level);
  EXPECT_EQ(65, end);
  EXPECT_EQ(1, level);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BiDiLineIteratorTest,
    ::testing::Values(base::i18n::TextDirection::LEFT_TO_RIGHT,
                      base::i18n::TextDirection::RIGHT_TO_LEFT));

}  // namespace
}  // namespace gfx
}  // namespace ui
