// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/base/text/bytes_formatting.h"

#include <stddef.h>
#include <stdint.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(BytesFormattingTest, GetByteDisplayUnits) {
  static const struct {
    int64_t bytes;
    DataUnits expected;
  } cases[] = {
      {0, DATA_UNITS_BYTE},
      {512, DATA_UNITS_BYTE},
      {10 * 1024, DATA_UNITS_KIBIBYTE},
      {10 * 1024 * 1024, DATA_UNITS_MEBIBYTE},
      {10LL * 1024 * 1024 * 1024, DATA_UNITS_GIBIBYTE},
      {10LL * 1024 * 1024 * 1024 * 1024, DATA_UNITS_TEBIBYTE},
      {~(1LL << 63), DATA_UNITS_PEBIBYTE},
  };

  for (size_t i = 0; i < std::size(cases); ++i)
    EXPECT_EQ(cases[i].expected, GetByteDisplayUnits(cases[i].bytes));
}

TEST(BytesFormattingTest, FormatBytes) {
  static const struct {
    int64_t bytes;
    DataUnits units;
    const char* expected;
    const char* expected_with_units;
  } cases[] = {
      // Expected behavior: we show one post-decimal digit when we have
      // under two pre-decimal digits, except in cases where it makes no
      // sense (zero or bytes).
      // Since we switch units once we cross the 1000 mark, this keeps
      // the display of file sizes or bytes consistently around three
      // digits.
      {0, DATA_UNITS_BYTE, "0", "0 B"},
      {512, DATA_UNITS_BYTE, "512", "512 B"},
      {512, DATA_UNITS_KIBIBYTE, "0.5", "0.5 KB"},
      {1024 * 1024, DATA_UNITS_KIBIBYTE, "1,024", "1,024 KB"},
      {1024 * 1024, DATA_UNITS_MEBIBYTE, "1.0", "1.0 MB"},
      {1024 * 1024 * 1024, DATA_UNITS_GIBIBYTE, "1.0", "1.0 GB"},
      {10LL * 1024 * 1024 * 1024, DATA_UNITS_GIBIBYTE, "10.0", "10.0 GB"},
      {99LL * 1024 * 1024 * 1024, DATA_UNITS_GIBIBYTE, "99.0", "99.0 GB"},
      {105LL * 1024 * 1024 * 1024, DATA_UNITS_GIBIBYTE, "105", "105 GB"},
      {105LL * 1024 * 1024 * 1024 + 500LL * 1024 * 1024, DATA_UNITS_GIBIBYTE,
       "105", "105 GB"},
      {~(1LL << 63), DATA_UNITS_GIBIBYTE, "8,589,934,592", "8,589,934,592 GB"},
      {~(1LL << 63), DATA_UNITS_PEBIBYTE, "8,192", "8,192 PB"},

      {99 * 1024 + 103, DATA_UNITS_KIBIBYTE, "99.1", "99.1 KB"},
      {1024 * 1024 + 103, DATA_UNITS_KIBIBYTE, "1,024", "1,024 KB"},
      {1024 * 1024 + 205 * 1024, DATA_UNITS_MEBIBYTE, "1.2", "1.2 MB"},
      {1024 * 1024 * 1024 + (927 * 1024 * 1024), DATA_UNITS_GIBIBYTE, "1.9",
       "1.9 GB"},
      {10LL * 1024 * 1024 * 1024, DATA_UNITS_GIBIBYTE, "10.0", "10.0 GB"},
      {100LL * 1024 * 1024 * 1024, DATA_UNITS_GIBIBYTE, "100", "100 GB"},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    EXPECT_EQ(base::ASCIIToUTF16(cases[i].expected),
              FormatBytesWithUnits(cases[i].bytes, cases[i].units, false));
    EXPECT_EQ(base::ASCIIToUTF16(cases[i].expected_with_units),
              FormatBytesWithUnits(cases[i].bytes, cases[i].units, true));
  }
}

}  // namespace ui
