// Copyright 2020 The Chromium Authors
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/utils.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace liburlpattern {

void RunEscapeRegexpStringTest(std::string_view input,
                               std::string_view expected) {
  std::string result = EscapeRegexpString(input);
  EXPECT_EQ(result, expected);
  EXPECT_EQ(EscapedRegexpStringLength(input), result.size());
}

TEST(UtilsTest, EscapeRegexpStringDot) {
  RunEscapeRegexpStringTest("index.html", "index\\.html");
}

TEST(UtilsTest, EscapeRegexpStringPlus) {
  RunEscapeRegexpStringTest("foo+", "foo\\+");
}

TEST(UtilsTest, EscapeRegexpStringStar) {
  RunEscapeRegexpStringTest("foo*", "foo\\*");
}

TEST(UtilsTest, EscapeRegexpStringQuestion) {
  RunEscapeRegexpStringTest("foo?", "foo\\?");
}

TEST(UtilsTest, EscapeRegexpStringEquals) {
  RunEscapeRegexpStringTest("foo=bar", "foo=bar");
}

TEST(UtilsTest, EscapeRegexpStringCaret) {
  RunEscapeRegexpStringTest("^foo", "\\^foo");
}

TEST(UtilsTest, EscapeRegexpStringBang) {
  RunEscapeRegexpStringTest("!foo", "!foo");
}

TEST(UtilsTest, EscapeRegexpStringColon) {
  RunEscapeRegexpStringTest(":foo", ":foo");
}

TEST(UtilsTest, EscapeRegexpStringDollar) {
  RunEscapeRegexpStringTest("foo$", "foo\\$");
}

TEST(UtilsTest, EscapeRegexpStringCurlyBraces) {
  RunEscapeRegexpStringTest("{foo}", "\\{foo\\}");
}

TEST(UtilsTest, EscapeRegexpStringParens) {
  RunEscapeRegexpStringTest("(foo)", "\\(foo\\)");
}

TEST(UtilsTest, EscapeRegexpStringSquareBrackets) {
  RunEscapeRegexpStringTest("[foo]", "\\[foo\\]");
}

TEST(UtilsTest, EscapeRegexpStringPipe) {
  RunEscapeRegexpStringTest("foo|bar", "foo\\|bar");
}

TEST(UtilsTest, EscapeRegexpStringSlash) {
  RunEscapeRegexpStringTest("/foo/bar", "\\/foo\\/bar");
}

TEST(UtilsTest, EscapeRegexpStringBackslash) {
  RunEscapeRegexpStringTest("\\d", "\\\\d");
}

void RunEscapePatternStringTest(std::string_view input,
                                std::string_view expected) {
  std::string result;
  EscapePatternStringAndAppend(input, result);
  EXPECT_EQ(result, expected);
}

TEST(UtilsTest, EscapePatternStringPlus) {
  RunEscapePatternStringTest("foo+", "foo\\+");
}

TEST(UtilsTest, EscapePatternStringStar) {
  RunEscapePatternStringTest("foo*", "foo\\*");
}

TEST(UtilsTest, EscapePatternStringQuestion) {
  RunEscapePatternStringTest("foo?", "foo\\?");
}

TEST(UtilsTest, EscapePatternStringColon) {
  RunEscapePatternStringTest("foo:", "foo\\:");
}

TEST(UtilsTest, EscapePatternStringBraces) {
  RunEscapePatternStringTest("foo{}", "foo\\{\\}");
}

TEST(UtilsTest, EscapePatternStringParens) {
  RunEscapePatternStringTest("foo()", "foo\\(\\)");
}

TEST(UtilsTest, EscapePatternStringBackslash) {
  RunEscapePatternStringTest("foo\\", "foo\\\\");
}

TEST(UtilsTest, EscapePatternStringSlash) {
  RunEscapePatternStringTest("foo/", "foo/");
}

}  // namespace liburlpattern
