// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/menu_label_accelerator_util.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(MenuLabelAcceleratorTest, GetMnemonic) {
  static const struct {
    const std::u16string label;
    const char16_t mneumonic;
  } cases[] = {
      {u"", 0},       {u"Exit", 0},     {u"E&xit", 'x'},
      {u"E&&xit", 0}, {u"E&xi&t", 'x'}, {u"Exit&", 0},
  };
  for (const auto& test : cases)
    EXPECT_EQ(GetMnemonic(test.label), test.mneumonic);
}

TEST(MenuLabelAcceleratorTest, EscapeMenuLabelAmpersands) {
  static const struct {
    const char* input;
    const char* output;
  } cases[] = {
      {"nothing", "nothing"},
      {"foo &bar", "foo &&bar"},
      {"foo &&bar", "foo &&&&bar"},
      {"foo &&&bar", "foo &&&&&&bar"},
      {"&foo bar", "&&foo bar"},
      {"&&foo bar", "&&&&foo bar"},
      {"&&&foo bar", "&&&&&&foo bar"},
      {"&foo &bar", "&&foo &&bar"},
      {"&&foo &&bar", "&&&&foo &&&&bar"},
      {"f&o&o ba&r", "f&&o&&o ba&&r"},
      {"foo_&_bar", "foo_&&_bar"},
      {"&_foo_bar_&", "&&_foo_bar_&&"},
  };

  for (const auto& test : cases) {
    std::u16string in = base::ASCIIToUTF16(test.input);
    std::u16string out = base::ASCIIToUTF16(test.output);
    EXPECT_EQ(out, EscapeMenuLabelAmpersands(in));
  }
}

}  // namespace ui
