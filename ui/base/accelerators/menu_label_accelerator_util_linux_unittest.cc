// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/accelerators/menu_label_accelerator_util_linux.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(MenuLabelAcceleratorTest, ConvertAcceleratorsFromWindowsStyle) {
  static const struct {
    const char* input;
    const char* output;
  } cases[] = {
    { "", "" },
    { "nothing", "nothing" },
    { "foo &bar", "foo _bar" },
    { "foo &&bar", "foo &bar" },
    { "foo &&&bar", "foo &_bar" },
    { "&foo &&bar", "_foo &bar" },
    { "&foo &bar", "_foo _bar" },
  };
  for (size_t i = 0; i < std::size(cases); ++i) {
    std::string result = ConvertAcceleratorsFromWindowsStyle(cases[i].input);
    EXPECT_EQ(cases[i].output, result);
  }
}

TEST(MenuLabelAcceleratorTest, RemoveWindowsStyleAccelerators) {
  static const struct {
    const char* input;
    const char* output;
  } cases[] = {
    { "", "" },
    { "nothing", "nothing" },
    { "foo &bar", "foo bar" },
    { "foo &&bar", "foo &bar" },
    { "foo &&&bar", "foo &bar" },
    { "&foo &&bar", "foo &bar" },
    { "&foo &bar", "foo bar" },
  };
  for (size_t i = 0; i < std::size(cases); ++i) {
    std::string result = RemoveWindowsStyleAccelerators(cases[i].input);
    EXPECT_EQ(cases[i].output, result);
  }
}

}  // namespace ui
