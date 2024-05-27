// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/url_file_parser.h"

#include <tuple>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui::clipboard_util::internal {
namespace {

using UrlFileParserTest = testing::Test;

TEST_F(UrlFileParserTest, Empty) {
  std::string file_contents = "";

  std::string result = ExtractURLFromURLFileContents(file_contents);

  EXPECT_EQ("", result);
}

TEST_F(UrlFileParserTest, Basic) {
  std::string file_contents = R"(
[InternetShortcut]
URL=http://www.google.com/
  )";

  std::string result = ExtractURLFromURLFileContents(file_contents);

  EXPECT_EQ("http://www.google.com/", result);
}

TEST_F(UrlFileParserTest, MissingInternetShortcutSection) {
  std::string file_contents = R"(
[SomethingElse]
URL=http://www.google.com/
  )";

  std::string result = ExtractURLFromURLFileContents(file_contents);

  EXPECT_EQ("", result);
}

TEST_F(UrlFileParserTest, MissingURLKey) {
  std::string file_contents = R"(
[InternetShortcut]
Prop3=19,2
  )";

  std::string result = ExtractURLFromURLFileContents(file_contents);

  EXPECT_EQ("", result);
}

// This is the contents from
// chrome/test/data/edge_profile/Favorites/Google.url
TEST_F(UrlFileParserTest, Real) {
  std::string file_contents = R"(
[DEFAULT]
BASEURL=http://www.google.com/
[{000214A0-0000-0000-C000-000000000046}]
Prop3=19,2
[InternetShortcut]
IDList=
URL=http://www.google.com/
IconFile=http://www.google.com/favicon.ico
IconIndex=1
HotKey=0
  )";

  std::string result = ExtractURLFromURLFileContents(file_contents);

  EXPECT_EQ("http://www.google.com/", result);
}

TEST_F(UrlFileParserTest, URLHasEquals) {
  std::string file_contents = R"(
[InternetShortcut]
URL=https://www.google.com/search?q=search
  )";

  std::string result = ExtractURLFromURLFileContents(file_contents);

  EXPECT_EQ("https://www.google.com/search?q=search", result);
}

// The following tests are parsing malformed contents. They test that the code
// behaves reasonably and does not crash.

TEST_F(UrlFileParserTest, MalformedTwoInternetShortcutSectionsFirstHasURL) {
  std::string file_contents = R"(
[InternetShortcut]
URL=http://www.google.com/
[InternetShortcut]
  )";

  std::ignore = ExtractURLFromURLFileContents(file_contents);
}

TEST_F(UrlFileParserTest, MalformedTwoInternetShortcutSectionsSecondHasURL) {
  std::string file_contents = R"(
[InternetShortcut]
[InternetShortcut]
URL=http://www.google.com/
  )";

  std::ignore = ExtractURLFromURLFileContents(file_contents);
}

TEST_F(UrlFileParserTest, MalformedTwoURLs) {
  std::string file_contents = R"(
[InternetShortcut]
URL=http://www.google.com/
URL=http://www.youtube.com/
  )";

  std::ignore = ExtractURLFromURLFileContents(file_contents);
}

}  // namespace
}  // namespace ui::clipboard_util::internal
