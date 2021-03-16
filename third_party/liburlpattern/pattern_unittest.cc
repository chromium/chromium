// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/liburlpattern/parse.h"

namespace {

absl::StatusOr<std::string> PassThrough(absl::string_view input) {
  return std::string(input);
}

}  // namespace

namespace liburlpattern {

void RunRegexTest(absl::string_view input,
                  absl::string_view expected_regex,
                  std::vector<std::string> expected_name_list,
                  Options options = Options()) {
  auto result = Parse(input, PassThrough, options);
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

TEST(PatternRegexTest, NameWithUnicode) {
  RunRegexTest(":fooßar", R"(^([^\/#\?]+?)[\/#\?]?$)", {"fooßar"});
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

TEST(PatternRegexTest, Wildcard) {
  RunRegexTest("*", R"(^(.*)[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, RegexWithOptionalModifier) {
  RunRegexTest("([a-z]+)?", R"(^([a-z]+)?[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, WildcardWithOptionalModifier) {
  RunRegexTest("*?", R"(^(.*)?[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, RegexWithPrefix) {
  RunRegexTest("/foo/([a-z]+)", R"(^\/foo(?:\/([a-z]+))[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, WildcardWithPrefix) {
  RunRegexTest("/foo/*", R"(^\/foo(?:\/(.*))[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, RegexWithPrefixAndOptionalModifier) {
  RunRegexTest("/foo/([a-z]+)?", R"(^\/foo(?:\/([a-z]+))?[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, WildcardWithPrefixAndOptionalModifier) {
  RunRegexTest("/foo/*?", R"(^\/foo(?:\/(.*))?[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, RegexWithPrefixAndOneOrMoreModifier) {
  RunRegexTest("/foo/([a-z]+)+",
               R"(^\/foo(?:\/((?:[a-z]+)(?:\/(?:[a-z]+))*))[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, WildcardWithPrefixAndOneOrMoreModifier) {
  RunRegexTest("/foo/*+", R"(^\/foo(?:\/((?:.*)(?:\/(?:.*))*))[\/#\?]?$)",
               {"0"});
}

TEST(PatternRegexTest, RegexWithPrefixAndZeroOrMoreModifier) {
  RunRegexTest("/foo/([a-z]+)*",
               R"(^\/foo(?:\/((?:[a-z]+)(?:\/(?:[a-z]+))*))?[\/#\?]?$)", {"0"});
}

TEST(PatternRegexTest, WildcardWithPrefixAndZeroOrMoreModifier) {
  RunRegexTest("/foo/**", R"(^\/foo(?:\/((?:.*)(?:\/(?:.*))*))?[\/#\?]?$)",
               {"0"});
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

void RunPatternStringTest(absl::string_view input,
                          absl::string_view expected_pattern_string) {
  auto result = Parse(input, PassThrough);
  ASSERT_TRUE(result.ok());
  auto& pattern = result.value();
  std::string pattern_string = pattern.GeneratePatternString();
  EXPECT_EQ(pattern_string, expected_pattern_string);

  // The computed pattern string should be valid and parse correctly.
  auto result2 = Parse(pattern_string, PassThrough);
  EXPECT_TRUE(result.ok());

  // The second Pattern object may or may not be identical to the first
  // due to normalization.  For example, stripping the unnecessary grouping
  // from a `{foo}` term.

  // Computing a second pattern string should result in an identical
  // value, however.
  std::string pattern_string2 = result2.value().GeneratePatternString();
  EXPECT_EQ(pattern_string2, pattern_string);
}

TEST(PatternStringTest, Fixed) {
  RunPatternStringTest("/foo/bar", "/foo/bar");
}

TEST(PatternStringTest, Group) {
  RunPatternStringTest("/foo/{bar}", "/foo/bar");
}

TEST(PatternStringTest, GroupWithRegexp) {
  RunPatternStringTest("/foo/{(bar)}", "/foo/(bar)");
}

TEST(PatternStringTest, GroupWithPrefixAndRegexp) {
  RunPatternStringTest("/foo/{b(ar)}", "/foo/{b(ar)}");
}

TEST(PatternStringTest, GroupWithDefaultPrefixAndRegexp) {
  RunPatternStringTest("/foo{/(bar)}", "/foo/(bar)");
}

TEST(PatternStringTest, GroupWithRegexpAndSuffix) {
  RunPatternStringTest("/foo/{(ba)r}", "/foo/{(ba)r}");
}

TEST(PatternStringTest, GroupWithDefaultPrefixRegexpAndSuffix) {
  RunPatternStringTest("/foo{/(ba)r}", "/foo{/(ba)r}");
}

TEST(PatternStringTest, GroupWithQuestionModifier) {
  RunPatternStringTest("/foo/{bar}?", "/foo/{bar}?");
}

TEST(PatternStringTest, GroupWithStarModifier) {
  RunPatternStringTest("/foo/{bar}*", "/foo/{bar}*");
}

TEST(PatternStringTest, GroupWithPlusModifier) {
  RunPatternStringTest("/foo/{bar}+", "/foo/{bar}+");
}

TEST(PatternStringTest, NamedGroup) {
  RunPatternStringTest("/foo/:bar", "/foo/:bar");
}

TEST(PatternStringTest, NamedGroupWithRegexp) {
  RunPatternStringTest("/foo/:bar(baz)", "/foo/:bar(baz)");
}

TEST(PatternStringTest, NamedGroupWithEquivalentRegexp) {
  RunPatternStringTest("/foo/:bar([^\\/#\\?]+?)", "/foo/:bar");
}

TEST(PatternStringTest, NamedGroupWithWildcardEquivalentRegexp) {
  RunPatternStringTest("/foo/:bar(.*)", "/foo/:bar(.*)");
}

TEST(PatternStringTest, NamedGroupWithQuestionModifier) {
  RunPatternStringTest("/foo/:bar?", "/foo/:bar?");
}

TEST(PatternStringTest, NamedGroupWithStarModifier) {
  RunPatternStringTest("/foo/:bar*", "/foo/:bar*");
}

TEST(PatternStringTest, NamedGroupWithPlusModifier) {
  RunPatternStringTest("/foo/:bar+", "/foo/:bar+");
}

TEST(PatternStringTest, Regexp) {
  RunPatternStringTest("/foo/(bar)", "/foo/(bar)");
}

TEST(PatternStringTest, RegexpWithQuestionModifier) {
  RunPatternStringTest("/foo/(bar)?", "/foo/(bar)?");
}

TEST(PatternStringTest, RegexpWithStarModifier) {
  RunPatternStringTest("/foo/(bar)*", "/foo/(bar)*");
}

TEST(PatternStringTest, RegexpWithPlusModifier) {
  RunPatternStringTest("/foo/(bar)+", "/foo/(bar)+");
}

TEST(PatternStringTest, Wildcard) {
  RunPatternStringTest("/foo/*", "/foo/*");
}

TEST(PatternStringTest, RegexpWildcardEquivalent) {
  RunPatternStringTest("/foo/(.*)", "/foo/*");
}

TEST(PatternStringTest, RegexpEscapedNonPatternChar) {
  RunPatternStringTest("/foo/\\bar", "/foo/bar");
}

TEST(PatternStringTest, RegexpEscapedPatternChar) {
  RunPatternStringTest("/foo/\\:bar", "/foo/\\:bar");
}

TEST(PatternStringTest, RegexpEscapedPatternCharInPrefix) {
  RunPatternStringTest("/foo/{\\:bar(foo)}", "/foo/{\\:bar(foo)}");
}

TEST(PatternStringTest, RegexpEscapedPatternCharInSuffix) {
  RunPatternStringTest("/foo/{(foo)\\:bar}", "/foo/{(foo)\\:bar}");
}

}  // namespace liburlpattern
