// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace liburlpattern {

TEST(UtilsTest, EscapeStringDot) {
  EXPECT_EQ(EscapeString("index.html"), "index\\.html");
}

TEST(UtilsTest, EscapeStringPlus) {
  EXPECT_EQ(EscapeString("foo+"), "foo\\+");
}

TEST(UtilsTest, EscapeStringStar) {
  EXPECT_EQ(EscapeString("foo*"), "foo\\*");
}

TEST(UtilsTest, EscapeStringQuestion) {
  EXPECT_EQ(EscapeString("foo?"), "foo\\?");
}

TEST(UtilsTest, EscapeStringEquals) {
  EXPECT_EQ(EscapeString("foo=bar"), "foo\\=bar");
}

TEST(UtilsTest, EscapeStringCaret) {
  EXPECT_EQ(EscapeString("^foo"), "\\^foo");
}

TEST(UtilsTest, EscapeStringBang) {
  EXPECT_EQ(EscapeString("!foo"), "\\!foo");
}

TEST(UtilsTest, EscapeStringColon) {
  EXPECT_EQ(EscapeString(":foo"), "\\:foo");
}

TEST(UtilsTest, EscapeStringDollar) {
  EXPECT_EQ(EscapeString("foo$"), "foo\\$");
}

TEST(UtilsTest, EscapeStringCurlyBraces) {
  EXPECT_EQ(EscapeString("{foo}"), "\\{foo\\}");
}

TEST(UtilsTest, EscapeStringParens) {
  EXPECT_EQ(EscapeString("(foo)"), "\\(foo\\)");
}

TEST(UtilsTest, EscapeStringSquareBrackets) {
  EXPECT_EQ(EscapeString("[foo]"), "\\[foo\\]");
}

TEST(UtilsTest, EscapeStringPipe) {
  EXPECT_EQ(EscapeString("foo|bar"), "foo\\|bar");
}

TEST(UtilsTest, EscapeStringSlash) {
  EXPECT_EQ(EscapeString("/foo/bar"), "\\/foo\\/bar");
}

TEST(UtilsTest, EscapeStringBackslash) {
  EXPECT_EQ(EscapeString("\\d"), "\\\\d");
}

}  // namespace liburlpattern
