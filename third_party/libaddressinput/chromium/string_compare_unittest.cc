// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/src/cpp/src/util/string_compare.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(ChromeStringCompareTest, IgnoreDiacritics) {
  i18n::addressinput::StringCompare sc;
  EXPECT_TRUE(sc.NaturalEquals("Texas", base::WideToUTF8(L"T\u00E9xas")));
}

TEST(ChromeStringCompareTest, IgnoreCapitalization) {
  i18n::addressinput::StringCompare sc;
  EXPECT_TRUE(sc.NaturalEquals("Texas", "teXas"));
}

TEST(ChromeStringCompareTest, DifferentStringAreDifferent) {
  i18n::addressinput::StringCompare sc;
  EXPECT_FALSE(sc.NaturalEquals("Texas", "California"));
}

}  // namespace
