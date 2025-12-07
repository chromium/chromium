// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/text/bytes_formatting.h"

#include <array>

#include "base/byte_count.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(BytesFormattingTest, GetByteDisplayUnits) {
  struct Cases {
    base::ByteCount bytes;
    DataUnits expected;
  };
  static const auto cases = std::to_array<Cases>({
      {base::ByteCount(0), DataUnits::kByte},
      {base::ByteCount(512), DataUnits::kByte},
      {base::KiB(10), DataUnits::kKibibyte},
      {base::MiB(10), DataUnits::kMebibyte},
      {base::GiB(10), DataUnits::kGibibyte},
      {base::TiB(10), DataUnits::kTebibyte},
      {base::ByteCount::Max(), DataUnits::kPebibyte},
  });

  for (const auto& test_case : cases) {
    EXPECT_EQ(test_case.expected, GetByteDisplayUnits(test_case.bytes));
  }
}

TEST(BytesFormattingTest, FormatBytes) {
  struct Cases {
    base::ByteCount bytes;
    DataUnits units;
    const char* expected;
    const char* expected_with_units;
  };
  static const auto cases = std::to_array<Cases>({
      // Expected behavior: we show one post-decimal digit when we have under
      // two pre-decimal digits, except in cases where it makes no sense (zero
      // or bytes).
      //
      // Since we switch units once we cross the 1000 mark, this keeps the
      // display of file sizes or bytes consistently around three digits.
      {base::ByteCount(0), DataUnits::kByte, "0", "0 B"},
      {base::ByteCount(512), DataUnits::kByte, "512", "512 B"},
      {base::ByteCount(512), DataUnits::kKibibyte, "0.5", "0.5 KB"},
      {base::MiB(1), DataUnits::kKibibyte, "1,024", "1,024 KB"},
      {base::MiB(1), DataUnits::kMebibyte, "1.0", "1.0 MB"},
      {base::GiB(1), DataUnits::kGibibyte, "1.0", "1.0 GB"},
      {base::GiB(10), DataUnits::kGibibyte, "10.0", "10.0 GB"},
      {base::GiB(99), DataUnits::kGibibyte, "99.0", "99.0 GB"},
      {base::GiB(105), DataUnits::kGibibyte, "105", "105 GB"},
      {base::GiB(105) + base::MiB(500), DataUnits::kGibibyte, "105", "105 GB"},
      {base::ByteCount::Max(), DataUnits::kGibibyte, "8,589,934,592",
       "8,589,934,592 GB"},
      {base::ByteCount::Max(), DataUnits::kPebibyte, "8,192", "8,192 PB"},

      {base::KiB(99) + base::ByteCount(103), DataUnits::kKibibyte, "99.1",
       "99.1 KB"},
      {base::MiB(1) + base::ByteCount(103), DataUnits::kKibibyte, "1,024",
       "1,024 KB"},
      {base::MiB(1) + base::KiB(205), DataUnits::kMebibyte, "1.2", "1.2 MB"},
      {base::GiB(1) + base::MiB(927), DataUnits::kGibibyte, "1.9", "1.9 GB"},
      {base::GiB(100), DataUnits::kGibibyte, "100", "100 GB"},
  });

  for (const auto& test_case : cases) {
    EXPECT_EQ(base::ASCIIToUTF16(test_case.expected),
              FormatBytesWithUnits(test_case.bytes, test_case.units, false));
    EXPECT_EQ(base::ASCIIToUTF16(test_case.expected_with_units),
              FormatBytesWithUnits(test_case.bytes, test_case.units, true));
  }
}

}  // namespace ui
