// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/libaddressinput/src/cpp/src/rule.h"

#include <stddef.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(RuleTest, CanonicalizeSubKeyTest) {
  i18n::addressinput::Rule rule;
  ASSERT_TRUE(rule.ParseSerializedRule(base::WideToUTF8(
      L"{ \"sub_keys\": \"FOO~BAR~B\u00C4Z~\u0415\u0416\","
      L"  \"sub_names\": \"Foolandia~Bartopolis~B\u00E4zmonia~"
                          L"\u0415\u0436ville\" }")));
  EXPECT_EQ(4U, rule.GetSubKeys().size());

  static const struct {
    const wchar_t* input;
    // If empty, expect failure.
    const wchar_t* output;
  } expectations[] = {
    { L"foo", L"FOO" },
    { L"Foo", L"FOO" },
    { L"FOO", L"FOO" },
    { L"F", L"" },
    { L"FOO2", L"" },
    { L"", L"" },
    { L"Bar", L"BAR" },
    { L"Bartopolis", L"BAR" },
    { L"BARTOPOLIS", L"BAR" },
    { L"BaRToPoLiS", L"BAR" },
    // Diacriticals.
    { L"B\u00C4Z", L"B\u00C4Z" },
    { L"BAZ", L"B\u00C4Z" },
    { L"B\u00E4zmonia", L"B\u00C4Z" },
    { L"bazmonia", L"B\u00C4Z" },
    // Non-ascii (Cyrillic) case sensitivity.
    { L"\u0415\u0416", L"\u0415\u0416" },
    { L"\u0415\u0416VILLE", L"\u0415\u0416" },
    { L"\u0435\u0436", L"\u0415\u0416" },
    { L"\u0435\u0436VILLE", L"\u0415\u0416" },
    { L"\u0435\u0436VILL", L"" },
  };

  const size_t num_cases = sizeof(expectations) / sizeof(expectations[0]);
  for (size_t i = 0; i < num_cases; ++i) {
    const std::string input(base::WideToUTF8(expectations[i].input));
    const std::string expected_output(base::WideToUTF8(expectations[i].output));
    std::string output;
    EXPECT_EQ(!expected_output.empty(),
              rule.CanonicalizeSubKey(input, true, &output))
        << "Failed for input " << input;
    EXPECT_EQ(expected_output, output);
  }
}
