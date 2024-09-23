// Copyright 2020 The Chromium Authors
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/pattern.h"

#include <optional>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/liburlpattern/parse.h"

namespace {

absl::StatusOr<std::string> PassThrough(std::string_view input) {
  return std::string(input);
}

}  // namespace

namespace liburlpattern {

void RunRegexTest(std::string_view input,
                  std::string_view expected_regex,
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

TEST(PatternRegexTest, NameWithZeroOrMoreModifier) {
  RunRegexTest(":foo*", R"(^((?:[^\/#\?]+?)*)[\/#\?]?$)", {"foo"});
}

TEST(PatternRegexTest, NameWithOneOrMoreModifier) {
  RunRegexTest(":foo+", R"(^((?:[^\/#\?]+?)+)[\/#\?]?$)", {"foo"});
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

void RunPatternStringTest(std::string_view input,
                          std::string_view expected_pattern_string) {
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
  RunPatternStringTest("/foo/{(bar)}", "/foo/{(bar)}");
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

TEST(PatternStringTest, SegmentWildcardWithoutName) {
  RunPatternStringTest("/foo/([^\\/#\\?]+?)", "/foo/([^\\/#\\?]+?)");
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

TEST(PatternStringTest, RegexpFollowedByWildcard) {
  RunPatternStringTest("(foo)(.*)", "(foo)(.*)");
}

TEST(PatternStringTest, RegexpWithOptionalModifierFollowedByWildcard) {
  RunPatternStringTest("(foo)?(.*)", "(foo)?*");
}

TEST(PatternStringTest, RegexpWithSuffixModifierFollowedByWildcard) {
  RunPatternStringTest("{(foo)a}(.*)", "{(foo)a}(.*)");
}

TEST(PatternStringTest, NamedGroupInGroupingFollowedByWildcard) {
  RunPatternStringTest("{:foo}(.*)", "{:foo}(.*)");
}

TEST(PatternStringTest, NamedGroupInGroupingFollowedByRegexp) {
  RunPatternStringTest("{:foo}(bar)", "{:foo}(bar)");
}

TEST(PatternStringTest, NamedGroupInGroupingFollowedByWildcardInGrouping) {
  RunPatternStringTest("{:foo}{(.*)}", "{:foo}(.*)");
}

TEST(PatternStringTest, NamedGroupInGroupingFollowedByWildcardWithSuffix) {
  RunPatternStringTest("{:foo}{(.*)bar}", ":foo{*bar}");
}

TEST(PatternStringTest, NamedGroupInGroupingFollowedByWildcardWithPrefix) {
  RunPatternStringTest("{:foo}{bar(.*)}", ":foo{bar*}");
}

TEST(PatternStringTest, NamedGroupInGroupingFollowedByWildcardWithCustomName) {
  RunPatternStringTest("{:foo}:bar(.*)", ":foo:bar(.*)");
}

TEST(PatternStringTest,
     NamedGroupInGroupingWithOptionalModifierFollowedByWildcard) {
  RunPatternStringTest("{:foo}?(.*)", ":foo?*");
}

TEST(PatternStringTest, NamedGroupWithEscapedValidNameSuffix) {
  RunPatternStringTest("{:foo\\bar}", "{:foo\\bar}");
}

TEST(PatternStringTest, NamedGroupWithEscapedInvalidNameSuffix) {
  RunPatternStringTest("{:foo\\.bar}", "{:foo.bar}");
}

TEST(PatternStringTest, NamedGroupInGroupingFollowedByValidNameText) {
  RunPatternStringTest("{:foo}bar", "{:foo}bar");
}

TEST(PatternStringTest, NamedGroupFollowedByEscapedValidNameText) {
  RunPatternStringTest(":foo\\bar", "{:foo}bar");
}

TEST(PatternStringTest, NamedGroupWithRegexpFollowedByValidNameText) {
  RunPatternStringTest(":foo(baz)bar", ":foo(baz)bar");
}

TEST(PatternStringTest, NamedGroupFollowedByEmptyGroupAndWildcard) {
  RunPatternStringTest(":foo{}(.*)", "{:foo}(.*)");
}

TEST(PatternStringTest, NamedGroupFollowedByEmptyGroupAndValidNameText) {
  RunPatternStringTest(":foo{}bar", "{:foo}bar");
}

TEST(PatternStringTest,
     NamedGroupFollowedByEmptyGroupWithOptionalModifierAndValidNameText) {
  RunPatternStringTest(":foo{}?bar", "{:foo}bar");
}

TEST(PatternStringTest, NamedGroupWithRegexpFollowedByWildcard) {
  RunPatternStringTest(":foo(bar)(.*)", ":foo(bar)(.*)");
}

TEST(PatternStringTest, NamedGroupWithRegexpAndValidNameSuffix) {
  RunPatternStringTest("{:foo(baz)bar}", "{:foo(baz)bar}");
}

TEST(PatternStringTest, WildcardSlashAndWildcard) {
  RunPatternStringTest("*/*", "*/*");
}

TEST(PatternStringTest, WildcardEscapedSlashAndWildcard) {
  // The backslash in the original input forces the `/` to not be an
  // implicit prefix for the second `*`.  The generated pattern string
  // must similarly prevent the implicit prefix from occuring.  This
  // is done using the `{}` grouping instead, however, as its a bit more
  // readable than escape backslashes.
  RunPatternStringTest("*\\/*", "*/{*}");
}

TEST(PatternStringTest, WildcardSlashAndWildcardInGrouping) {
  RunPatternStringTest("*/{*}", "*/{*}");
}

TEST(PatternStringTest, WildcardSlashSlashAndWildcard) {
  RunPatternStringTest("*//*", "*//*");
}

TEST(
    PatternStringTest,
    WildcardFollowedByEmptyGroupWithZeroOrMoreModifierAndWildcardWithOptionalModifier) {
  RunPatternStringTest("*{}**?", "*(.*)?");
}

TEST(PatternStringTest, CaseFromFuzzer) {
  RunPatternStringTest(".:bax\\a*{}**", "{.:bax}a*(.*)");
}

struct DirectMatchCase {
  std::string_view input;
  bool expected_match = true;
  std::vector<std::pair<std::string_view, std::optional<std::string_view>>>
      expected_groups;
};

void RunDirectMatchTest(std::string_view input,
                        std::vector<DirectMatchCase> case_list) {
  auto result =
      Parse(input, PassThrough,
            {.sensitive = true, .strict = true, .end = true, .start = true});
  ASSERT_TRUE(result.ok());
  auto& pattern = result.value();
  EXPECT_TRUE(pattern.CanDirectMatch());
  for (const auto& c : case_list) {
    std::vector<std::pair<std::string_view, std::optional<std::string_view>>>
        matched_groups;
    EXPECT_EQ(c.expected_match, pattern.DirectMatch(c.input, &matched_groups));
    ASSERT_EQ(c.expected_groups.size(), matched_groups.size());
    for (size_t i = 0; i < matched_groups.size(); ++i) {
      EXPECT_EQ(c.expected_groups[i], matched_groups[i]);
    }
  }
}

void RunDirectMatchUnsupportedTest(std::string_view input,
                                   Options options = {.sensitive = true,
                                                      .strict = true,
                                                      .end = true,
                                                      .start = true}) {
  auto result = Parse(input, PassThrough, options);
  ASSERT_TRUE(result.ok());
  auto& pattern = result.value();
  EXPECT_FALSE(pattern.CanDirectMatch());
}

TEST(PatternDirectMatch, FullWildcardSupported) {
  RunDirectMatchTest("*",
                     {
                         {.input = "/foo", .expected_groups = {{"0", "/foo"}}},
                         {.input = "", .expected_groups = {{"0", ""}}},
                     });
}

TEST(PatternDirectMatch, FullWildcardInGroupSupported) {
  RunDirectMatchTest("{*}",
                     {
                         {.input = "/foo", .expected_groups = {{"0", "/foo"}}},
                         {.input = "", .expected_groups = {{"0", ""}}},
                     });
}

TEST(PatternDirectMatch, FullWildcardUsingRegexSupported) {
  RunDirectMatchTest("(.*)",
                     {
                         {.input = "/foo", .expected_groups = {{"0", "/foo"}}},
                         {.input = "", .expected_groups = {{"0", ""}}},
                     });
}

TEST(PatternDirectMatch, FullWildcardAndOptionalModifierSupported) {
  RunDirectMatchTest("*?",
                     {
                         {.input = "/foo", .expected_groups = {{"0", "/foo"}}},
                         {.input = "", .expected_groups = {{"0", ""}}},
                     });
}

TEST(PatternDirectMatch, FullWildcardAndOneOrMoreModifierSupported) {
  RunDirectMatchTest("*+",
                     {
                         {.input = "/foo", .expected_groups = {{"0", "/foo"}}},
                         {.input = "", .expected_groups = {{"0", ""}}},
                     });
}

TEST(PatternDirectMatch, FullWildcardAndZeroOrMoreModifierSupported) {
  RunDirectMatchTest("**",
                     {
                         {.input = "/foo", .expected_groups = {{"0", "/foo"}}},
                         {.input = "", .expected_groups = {{"0", ""}}},
                     });
}

TEST(PatternDirectMatch, FullWildcardWithNameUsingRegexSupported) {
  RunDirectMatchTest(
      ":name(.*)", {
                       {.input = "/foo", .expected_groups = {{"name", "/foo"}}},
                       {.input = "", .expected_groups = {{"name", ""}}},
                   });
}

TEST(PatternDirectMatch, OptionsFalseStartUnsupported) {
  RunDirectMatchUnsupportedTest(
      "*", {.sensitive = true, .strict = true, .end = true, .start = false});
}

TEST(PatternDirectMatch, OptionsFalseEndUnsupported) {
  RunDirectMatchUnsupportedTest(
      "*", {.sensitive = true, .strict = true, .end = false, .start = true});
}

TEST(PatternDirectMatch, OptionsFalseStrictUnsupported) {
  RunDirectMatchUnsupportedTest(
      "*", {.sensitive = true, .strict = false, .end = true, .start = true});
}

TEST(PatternDirectMatch, OptionsFalseSensitiveUnsupported) {
  RunDirectMatchUnsupportedTest(
      "*", {.sensitive = false, .strict = true, .end = true, .start = true});
}

TEST(PatternDirectMatch, FullWildcardWithPrefixUnsupported) {
  RunDirectMatchUnsupportedTest("/*");
}

TEST(PatternDirectMatch, FullWildcardInGroupWithPrefixUnsupported) {
  RunDirectMatchUnsupportedTest("{foo*}");
}

TEST(PatternDirectMatch, FullWildcardInGroupWithSuffixUnsupported) {
  RunDirectMatchUnsupportedTest("{*foo}");
}

TEST(PatternDirectMatch, EmptyPatternSupported) {
  RunDirectMatchTest("", {
                             {.input = "", .expected_groups = {}},
                             {.input = "/", .expected_match = false},
                         });
}

TEST(PatternDirectMatch, FixedTextSupported) {
  RunDirectMatchTest("foo", {
                                {.input = "foo", .expected_groups = {}},
                                {.input = "fo", .expected_match = false},
                                {.input = "foobar", .expected_match = false},
                            });
}

TEST(PatternDirectMatch, FixedTextInGroupSupported) {
  RunDirectMatchTest("{foo}", {
                                  {.input = "foo", .expected_groups = {}},
                                  {.input = "fo", .expected_match = false},
                                  {.input = "foobar", .expected_match = false},
                              });
}

TEST(PatternDirectMatch, FixedTextInGroupWithOptionalModifierUnsupported) {
  RunDirectMatchUnsupportedTest("{foo}?");
}

TEST(PatternDirectMatch, FixedTextInGroupWithZeroOrMoreModifierUnsupported) {
  RunDirectMatchUnsupportedTest("{foo}*");
}

TEST(PatternDirectMatch, FixedTextInGroupWithOneOrMoreModifierUnsupported) {
  RunDirectMatchUnsupportedTest("{foo}+");
}

TEST(PatternDirectMatch, FixedTextAndFullWildcardUnsupported) {
  RunDirectMatchUnsupportedTest("/foo*");
}

TEST(PatternDirectMatch, NamedGroupUnsupported) {
  RunDirectMatchUnsupportedTest(":name");
}

TEST(PatternDirectMatch, RegexUnsupported) {
  RunDirectMatchUnsupportedTest("(foo)");
}

}  // namespace liburlpattern
