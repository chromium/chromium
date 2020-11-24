// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/liburlpattern/parse.h"

namespace liburlpattern {

void RunRegexTest(absl::string_view input,
                  absl::string_view expected_regex,
                  std::vector<std::string> expected_name_list,
                  Options options = Options()) {
  auto result = Parse(input, options);
  ASSERT_TRUE(result.ok());
  auto& pattern = result.value();
  std::vector<std::string> name_list;
  std::string regex = pattern.GenerateRegexString(&name_list);
  EXPECT_EQ(regex, expected_regex);
  EXPECT_EQ(name_list, expected_name_list);
}

// The following expected test case values were generated using path-to-regexp
// 6.2.0.

TEST(PatternRegexTest, Fixed) {
  RunRegexTest("/foo/bar", R"(^\/foo\/bar[\/#\?]?$)", {});
}

TEST(PatternRegexTest, FixedWithModifier) {
  RunRegexTest("{/foo/bar}?", R"(^(?:\/foo\/bar)?[\/#\?]?$)", {});
}

TEST(PatternRegexTest, Name) {
  RunRegexTest(":foo", R"(^([^\/#\?]+?)[\/#\?]?$)", {"foo"});
}

TEST(PatternRegexTest, NameWithOptionalModifier) {
  RunRegexTest(":foo?", R"(^([^\/#\?]+?)?[\/#\?]?$)", {"foo"});
}

TEST(PatternRegexTest, NameWithPrefix) {
  RunRegexTest("/foo/:bar", R"(^\/foo(?:\/([^\/#\?]+?))[\/#\?]?$)", {"bar"});
}

TEST(PatternRegexTest, NameWithPrefixAndOptionalModifier) {
  RunRegexTest("/foo/:bar?", R"(^\/foo(?:\/([^\/#\?]+?))?[\/#\?]?$)", {"bar"});
}

TEST(PatternRegexTest, NameWithPrefixAndOneOrMoreModifier) {
  RunRegexTest("/foo/:bar+",
               R"(^\/foo(?:\/((?:[^\/#\?]+?)(?:\/(?:[^\/#\?]+?))*))[\/#\?]?$)",
               {"bar"});
}

TEST(PatternRegexTest, NameWithPrefixAndZeroOrMoreModifier) {
  RunRegexTest("/foo/:bar*",
               R"(^\/foo(?:\/((?:[^\/#\?]+?)(?:\/(?:[^\/#\?]+?))*))?[\/#\?]?$)",
               {"bar"});
}

TEST(PatternRegexTest, Regex) {
  RunRegexTest("([a-z]+)", R"(^([a-z]+)[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, RegexFullWildcard) {
  RunRegexTest("(.*)", R"(^(.*)[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, RegexWithOptionalModifier) {
  RunRegexTest("([a-z]+)?", R"(^([a-z]+)?[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, RegexWithPrefix) {
  RunRegexTest("/foo/([a-z]+)", R"(^\/foo(?:\/([a-z]+))[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, RegexWithPrefixAndOptionalModifier) {
  RunRegexTest("/foo/([a-z]+)?", R"(^\/foo(?:\/([a-z]+))?[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, RegexWithPrefixAndOneOrMoreModifier) {
  RunRegexTest("/foo/([a-z]+)+",
               R"(^\/foo(?:\/((?:[a-z]+)(?:\/(?:[a-z]+))*))[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, RegexWithPrefixAndZeroOrMoreModifier) {
  RunRegexTest("/foo/([a-z]+)*",
               R"(^\/foo(?:\/((?:[a-z]+)(?:\/(?:[a-z]+))*))?[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, NameWithCustomRegex) {
  RunRegexTest(":foo([a-z]+)", R"(^([a-z]+)[\/#\?]?$)", {"foo"});
}

TEST(PatternRegexTest, ManyGroups) {
  RunRegexTest("/([a-z])+/:foo/([a-z])+/:bar+",
               R"(^(?:\/((?:[a-z])(?:\/(?:[a-z]))*))(?:\/([^\/#\?]+?))(?:\/)"
               R"(((?:[a-z])(?:\/(?:[a-z]))*))(?:\/((?:[^\/#\?]+?)(?:\/)"
               R"((?:[^\/#\?]+?))*))[\/#\?]?$)",
               {"0", "foo", "1", "bar"});
}

TEST(PatternRegexTest, StrictNameWithPrefixAndOneOrMoreModifier) {
  RunRegexTest("/foo/:bar+",
               R"(^\/foo(?:\/((?:[^\/#\?]+?)(?:\/(?:[^\/#\?]+?))*))$)", {"bar"},
               {.strict = true});
}

TEST(PatternRegexTest, NoEndNameWithPrefixAndOneOrMoreModifier) {
  RunRegexTest("/foo/:bar+",
               R"(^\/foo(?:\/((?:[^\/#\?]+?)(?:\/(?:[^\/#\?]+?))*))(?:[\/)"
               R"(#\?](?=[]|$))?(?=[\/#\?]|[]|$))",
               {"bar"}, {.end = false});
}

TEST(PatternRegexTest, NoEndFixedWithTrailingDelimiter) {
  RunRegexTest("/foo/bar/", R"(^\/foo\/bar\/(?:[\/#\?](?=[]|$))?)", {},
               {.end = false});
}

TEST(PatternRegexTest, StrictNoEndFixedWithTrailingDelimiter) {
  RunRegexTest("/foo/bar/", R"(^\/foo\/bar\/)", {},
               {.strict = true, .end = false});
}

TEST(PatternRegexTest, StrictNoEndFixedWithoutTrailingDelimiter) {
  RunRegexTest("/foo/bar", R"(^\/foo\/bar(?=[\/#\?]|[]|$))", {},
               {.strict = true, .end = false});
}

TEST(PatternRegexTest, NoStartNameWithPrefixAndOneOrMoreModifier) {
  RunRegexTest("/foo/:bar+",
               R"(\/foo(?:\/((?:[^\/#\?]+?)(?:\/(?:[^\/#\?]+?))*))[\/#\?]?$)",
               {"bar"}, {.start = false});
}

TEST(PatternRegexTest, EndsWithNameWithPrefixAndOneOrMoreModifier) {
  RunRegexTest("/foo/:bar+",
               R"(^\/foo(?:\/((?:[^\/#\?]+?)(?:\/(?:[^\/#\?]+?))*))[\/)"
               R"(#\?]?(?=[#]|$))",
               {"bar"}, {.ends_with = "#"});
}

TEST(PatternRegexTest, EndsWithNoEndNameWithPrefixAndOneOrMoreModifier) {
  RunRegexTest("/foo/:bar+",
               R"(^\/foo(?:\/((?:[^\/#\?]+?)(?:\/(?:[^\/#\?]+?))*))(?:[\/)"
               R"(#\?](?=[#]|$))?(?=[\/#\?]|[#]|$))",
               {"bar"}, {.end = false, .ends_with = "#"});
}

}  // namespace liburlpattern
