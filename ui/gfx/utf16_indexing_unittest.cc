// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/utf16_indexing.h"

namespace gfx {

TEST(UTF16IndexingTest, IndexOffsetConversions) {
  // Valid surrogate pair surrounded by unpaired surrogates
  const char16_t foo[] = {0xDC00, 0xD800, 0xD800, 0xDFFF, 0xDFFF, 0xDBFF, 0};
  const std::u16string s(foo);
  const size_t the_invalid_index = 3;
  for (size_t i = 0; i <= s.length(); ++i)
    EXPECT_EQ(i != the_invalid_index, IsValidCodePointIndex(s, i));
  for (size_t i = 0; i <= s.length(); ++i) {
    for (size_t j = i; j <= s.length(); ++j) {
      ptrdiff_t offset = static_cast<ptrdiff_t>(j - i);
      if (i <= the_invalid_index && j > the_invalid_index)
        --offset;
      EXPECT_EQ(offset, UTF16IndexToOffset(s, i, j));
      EXPECT_EQ(-offset, UTF16IndexToOffset(s, j, i));
      size_t adjusted_j = (j == the_invalid_index) ? j + 1 : j;
      EXPECT_EQ(adjusted_j, UTF16OffsetToIndex(s, i, offset));
      size_t adjusted_i = (i == the_invalid_index) ? i + 1 : i;
      EXPECT_EQ(adjusted_i, UTF16OffsetToIndex(s, j, -offset));
    }
  }
}

}  // namespace gfx
