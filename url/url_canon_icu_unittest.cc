// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/350788890): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "url/url_canon_icu.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/ucnv.h"
#include "url/url_canon.h"
#include "url/url_canon_icu_test_helpers.h"
#include "url/url_canon_stdstring.h"
#include "url/url_test_utils.h"

namespace url {

namespace {

TEST(URLCanonIcuTest, ICUCharsetConverter) {
  struct ICUCase {
    const wchar_t* input;
    const char* encoding;
    const char* expected;
  } icu_cases[] = {
      // UTF-8.
    {L"Hello, world", "utf-8", "Hello, world"},
    {L"\x4f60\x597d", "utf-8", "\xe4\xbd\xa0\xe5\xa5\xbd"},
      // Non-BMP UTF-8.
    {L"!\xd800\xdf00!", "utf-8", "!\xf0\x90\x8c\x80!"},
      // Big5
    {L"\x4f60\x597d", "big5", "\xa7\x41\xa6\x6e"},
      // Unrepresentable character in the destination set.
    {L"hello\x4f60\x06de\x597dworld", "big5",
      "hello\xa7\x41%26%231758%3B\xa6\x6eworld"},
  };

  for (size_t i = 0; i < std::size(icu_cases); i++) {
    test::UConvScoper conv(icu_cases[i].encoding);
    ASSERT_TRUE(conv.converter() != NULL);
    ICUCharsetConverter converter(conv.converter());

    std::string str;
    StdStringCanonOutput output(&str);

    std::u16string input_str(
        test_utils::TruncateWStringToUTF16(icu_cases[i].input));
    int input_len = static_cast<int>(input_str.length());
    converter.ConvertFromUTF16(input_str.c_str(), input_len, &output);
    output.Complete();

    EXPECT_STREQ(icu_cases[i].expected, str.c_str());
  }

  // Test string sizes around the resize boundary for the output to make sure
  // the converter resizes as needed.
  const int static_size = 16;
  test::UConvScoper conv("utf-8");
  ASSERT_TRUE(conv.converter());
  ICUCharsetConverter converter(conv.converter());
  for (int i = static_size - 2; i <= static_size + 2; i++) {
    // Make a string with the appropriate length.
    std::u16string input;
    for (int ch = 0; ch < i; ch++)
      input.push_back('a');

    RawCanonOutput<static_size> output;
    converter.ConvertFromUTF16(input.c_str(), static_cast<int>(input.length()),
                               &output);
    EXPECT_EQ(input.length(), output.length());
  }
}

TEST(URLCanonIcuTest, QueryWithConverter) {
  struct QueryCase {
    const char* input8;
    const wchar_t* input16;
    const char* encoding;
    const char* expected;
  } query_cases[] = {
      // Regular ASCII case in some different encodings.
    {"foo=bar", L"foo=bar", "utf-8", "?foo=bar"},
    {"foo=bar", L"foo=bar", "shift_jis", "?foo=bar"},
    {"foo=bar", L"foo=bar", "gb2312", "?foo=bar"},
      // Chinese input/output
    {"q=\xe4\xbd\xa0\xe5\xa5\xbd", L"q=\x4f60\x597d", "gb2312",
      "?q=%C4%E3%BA%C3"},
    {"q=\xe4\xbd\xa0\xe5\xa5\xbd", L"q=\x4f60\x597d", "big5", "?q=%A7A%A6n"},
      // Unencodable character in the destination character set should be
      // escaped. The escape sequence unescapes to be the entity name:
      // "?q=&#20320;"
    {"q=Chinese\xef\xbc\xa7", L"q=Chinese\xff27", "iso-8859-1",
      "?q=Chinese%26%2365319%3B"},
  };

  for (size_t i = 0; i < std::size(query_cases); i++) {
    Component out_comp;

    test::UConvScoper conv(query_cases[i].encoding);
    ASSERT_TRUE(!query_cases[i].encoding || conv.converter());
    ICUCharsetConverter converter(conv.converter());

    if (query_cases[i].input8) {
      int len = static_cast<int>(strlen(query_cases[i].input8));
      Component in_comp(0, len);
      std::string out_str;

      StdStringCanonOutput output(&out_str);
      CanonicalizeQuery(query_cases[i].input8, in_comp, &converter, &output,
                        &out_comp);
      output.Complete();

      EXPECT_EQ(query_cases[i].expected, out_str);
    }

    if (query_cases[i].input16) {
      std::u16string input16(
          test_utils::TruncateWStringToUTF16(query_cases[i].input16));
      int len = static_cast<int>(input16.length());
      Component in_comp(0, len);
      std::string out_str;

      StdStringCanonOutput output(&out_str);
      CanonicalizeQuery(input16.c_str(), in_comp, &converter, &output,
                        &out_comp);
      output.Complete();

      EXPECT_EQ(query_cases[i].expected, out_str);
    }
  }

  // Extra test for input with embedded NULL;
  std::string out_str;
  StdStringCanonOutput output(&out_str);
  Component out_comp;
  CanonicalizeQuery("a \x00z\x01", Component(0, 5), NULL, &output, &out_comp);
  output.Complete();
  EXPECT_EQ("?a%20%00z%01", out_str);
}

}  // namespace

}  // namespace url
