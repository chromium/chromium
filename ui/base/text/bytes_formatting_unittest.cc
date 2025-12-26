// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/text/bytes_formatting.h"

#include <array>

#include "base/byte_size.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(BytesFormattingTest, GetByteDisplayUnits) {
  struct Cases {
    base::ByteSize bytes;
    DataUnits expected;
  };
  static const auto cases = std::to_array<Cases>({
      {base::ByteSize(0), DataUnits::kByte},
      {base::ByteSize(512), DataUnits::kByte},
      {base::KiBU(10), DataUnits::kKibibyte},
      {base::MiBU(10), DataUnits::kMebibyte},
      {base::GiBU(10), DataUnits::kGibibyte},
      {base::TiBU(10), DataUnits::kTebibyte},
      {base::ByteSize::Max(), DataUnits::kPebibyte},
  });

  for (const auto& test_case : cases) {
    EXPECT_EQ(test_case.expected, GetByteDisplayUnits(test_case.bytes));
  }
}

TEST(BytesFormattingTest, FormatBytes) {
  struct Cases {
    base::ByteSize bytes;
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
      {base::ByteSize(0), DataUnits::kByte, "0", "0 B"},
      {base::ByteSize(512), DataUnits::kByte, "512", "512 B"},
      {base::ByteSize(512), DataUnits::kKibibyte, "0.5", "0.5 KB"},
      {base::MiBU(1), DataUnits::kKibibyte, "1,024", "1,024 KB"},
      {base::MiBU(1), DataUnits::kMebibyte, "1.0", "1.0 MB"},
      {base::GiBU(1), DataUnits::kGibibyte, "1.0", "1.0 GB"},
      {base::GiBU(10), DataUnits::kGibibyte, "10.0", "10.0 GB"},
      {base::GiBU(99), DataUnits::kGibibyte, "99.0", "99.0 GB"},
      {base::GiBU(105), DataUnits::kGibibyte, "105", "105 GB"},
      {base::GiBU(105) + base::MiBU(500), DataUnits::kGibibyte, "105",
       "105 GB"},
      {base::ByteSize::Max(), DataUnits::kGibibyte, "8,589,934,592",
       "8,589,934,592 GB"},
      {base::ByteSize::Max(), DataUnits::kPebibyte, "8,192", "8,192 PB"},

      {base::KiBU(99) + base::ByteSize(103), DataUnits::kKibibyte, "99.1",
       "99.1 KB"},
      {base::MiBU(1) + base::ByteSize(103), DataUnits::kKibibyte, "1,024",
       "1,024 KB"},
      {base::MiBU(1) + base::KiBU(205), DataUnits::kMebibyte, "1.2", "1.2 MB"},
      {base::GiBU(1) + base::MiBU(927), DataUnits::kGibibyte, "1.9", "1.9 GB"},
      {base::GiBU(100), DataUnits::kGibibyte, "100", "100 GB"},
  });

  for (const auto& test_case : cases) {
    EXPECT_EQ(base::ASCIIToUTF16(test_case.expected),
              FormatBytesWithUnits(test_case.bytes, test_case.units, false));
    EXPECT_EQ(base::ASCIIToUTF16(test_case.expected_with_units),
              FormatBytesWithUnits(test_case.bytes, test_case.units, true));
  }
}

}  // namespace ui
