// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace liburlpattern {

void RunEscapeStringTest(absl::string_view input, absl::string_view expected) {
  std::string result = EscapeString(input);
  EXPECT_EQ(result, expected);
  EXPECT_EQ(EscapedLength(input), result.size());
}

TEST(UtilsTest, EscapeStringDot) {
  RunEscapeStringTest("index.html", "index\\.html");
}

TEST(UtilsTest, EscapeStringPlus) {
  RunEscapeStringTest("foo+", "foo\\+");
}

TEST(UtilsTest, EscapeStringStar) {
  RunEscapeStringTest("foo*", "foo\\*");
}

TEST(UtilsTest, EscapeStringQuestion) {
  RunEscapeStringTest("foo?", "foo\\?");
}

TEST(UtilsTest, EscapeStringEquals) {
  RunEscapeStringTest("foo=bar", "foo\\=bar");
}

TEST(UtilsTest, EscapeStringCaret) {
  RunEscapeStringTest("^foo", "\\^foo");
}

TEST(UtilsTest, EscapeStringBang) {
  RunEscapeStringTest("!foo", "\\!foo");
}

TEST(UtilsTest, EscapeStringColon) {
  RunEscapeStringTest(":foo", "\\:foo");
}

TEST(UtilsTest, EscapeStringDollar) {
  RunEscapeStringTest("foo$", "foo\\$");
}

TEST(UtilsTest, EscapeStringCurlyBraces) {
  RunEscapeStringTest("{foo}", "\\{foo\\}");
}

TEST(UtilsTest, EscapeStringParens) {
  RunEscapeStringTest("(foo)", "\\(foo\\)");
}

TEST(UtilsTest, EscapeStringSquareBrackets) {
  RunEscapeStringTest("[foo]", "\\[foo\\]");
}

TEST(UtilsTest, EscapeStringPipe) {
  RunEscapeStringTest("foo|bar", "foo\\|bar");
}

TEST(UtilsTest, EscapeStringSlash) {
  RunEscapeStringTest("/foo/bar", "\\/foo\\/bar");
}

TEST(UtilsTest, EscapeStringBackslash) {
  RunEscapeStringTest("\\d", "\\\\d");
}

}  // namespace liburlpattern
