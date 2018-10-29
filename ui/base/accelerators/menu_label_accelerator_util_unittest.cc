// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/menu_label_accelerator_util.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(MenuLabelAcceleratorTest, GetMnemonic) {
  static const struct {
    const base::string16 label;
    const base::char16 mneumonic;
  } cases[] = {
      {base::ASCIIToUTF16(""), 0},         {base::ASCIIToUTF16("Exit"), 0},
      {base::ASCIIToUTF16("E&xit"), 'x'},  {base::ASCIIToUTF16("E&&xit"), 0},
      {base::ASCIIToUTF16("E&xi&t"), 'x'}, {base::ASCIIToUTF16("Exit&"), 0},
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
    base::string16 in = base::ASCIIToUTF16(test.input);
    base::string16 out = base::ASCIIToUTF16(test.output);
    EXPECT_EQ(out, EscapeMenuLabelAmpersands(in));
  }
}

}  // namespace ui
