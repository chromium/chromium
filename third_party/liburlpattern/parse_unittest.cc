// Copyright 2020 The Chromium Authors
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/parse.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/liburlpattern/pattern.h"

namespace {

absl::StatusOr<std::string> PassThrough(std::string_view input) {
  return std::string(input);
}

}  // namespace

namespace liburlpattern {

absl::StatusOr<std::string> ToUpper(std::string_view input) {
  std::string output;
  std::transform(input.begin(), input.end(), std::back_inserter(output),
                 [](unsigned char c) { return std::toupper(c); });
  return output;
}

void RunParseTest(std::string_view pattern,
                  absl::StatusOr<std::vector<Part>> expected,
                  EncodeCallback callback = PassThrough) {
  auto result = Parse(pattern, std::move(callback));
  ASSERT_EQ(result.ok(), expected.ok())
      << "parse status '" << result.status() << "' for: " << pattern;
  if (!expected.ok()) {
    ASSERT_EQ(result.status().code(), expected.status().code())
        << "parse status code for: " << pattern;
    EXPECT_NE(result.status().message().find(expected.status().message()),
              std::string::npos)
        << "parse message '" << result.status().message()
        << "' does not contain '" << expected.status().message()
        << "' for: " << pattern;
    return;
  }
  const auto& expected_part_list = expected.value();
  const auto& part_list = result.value().PartList();
  EXPECT_EQ(part_list.size(), expected_part_list.size())
      << "parser should produce expected number of parts for: " << pattern;
  for (size_t i = 0; i < part_list.size() && i < expected_part_list.size();
       ++i) {
    EXPECT_EQ(part_list[i], expected_part_list[i])
        << "token at index " << i << " wrong for: " << pattern;
  }
}

TEST(ParseTest, EmptyPattern) {
  RunParseTest("", std::vector<Part>());
}

TEST(ParseTest, EncoderCallback) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/FOO/BAR", Modifier::kNone),
  };
  RunParseTest("/foo/bar", expected_parts, ToUpper);
}

TEST(ParseTest, Fixed) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
  };
  RunParseTest("/foo", expected_parts);
}

TEST(ParseTest, FixedInGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
  };
  RunParseTest("{/foo}", expected_parts);
}

TEST(ParseTest, FixedAndFixedInGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
  };
  RunParseTest("/{foo}", expected_parts);
}

TEST(ParseTest, FixedInGroupAndFixed) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
  };
  RunParseTest("{/}foo", expected_parts);
}

TEST(ParseTest, FixedInGroupAndFixedInGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
  };
  RunParseTest("{/}{foo}", expected_parts);
}

TEST(ParseTest, FixedAndEmptyGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
  };
  RunParseTest("/f{}oo", expected_parts);
}

TEST(ParseTest, FixedInGroupWithOptionalModifier) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kOptional),
  };
  RunParseTest("{/foo}?", expected_parts);
}

TEST(ParseTest, FixedInGroupWithZeroOrMoreModifier) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kZeroOrMore),
  };
  RunParseTest("{/foo}*", expected_parts);
}

TEST(ParseTest, FixedInGroupWithOneOrMoreModifier) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kOneOrMore),
  };
  RunParseTest("{/foo}+", expected_parts);
}

TEST(ParseTest, FixedInEarlyTerminatedGroup) {
  RunParseTest("{/foo", absl::InvalidArgumentError("expected '}'"));
}

TEST(ParseTest, FixedInUnbalancedGroup) {
  RunParseTest("{/foo?", absl::InvalidArgumentError("expected '}'"));
}

TEST(ParseTest, FixedWithModifier) {
  RunParseTest("/foo?", absl::InvalidArgumentError("Unexpected modifier"));
}

TEST(ParseTest, Regex) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/f", Modifier::kNone),
      Part(PartType::kRegex, /*name=*/"0", /*prefix=*/"", "oo", /*suffix=*/"",
           Modifier::kNone),
  };
  RunParseTest("/f(oo)", expected_parts);
}

TEST(ParseTest, RegexInGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/f", Modifier::kNone),
      Part(PartType::kRegex, /*name=*/"0", /*prefix=*/"", "oo", /*suffix=*/"",
           Modifier::kNone),
  };
  RunParseTest("/f{(oo)}", expected_parts);
}

TEST(ParseTest, RegexWithPrefixAndSuffixInGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/", Modifier::kNone),
      Part(PartType::kRegex, /*name=*/"0", /*prefix=*/"f", "o", /*suffix=*/"o",
           Modifier::kNone),
  };
  RunParseTest("/{f(o)o}", expected_parts);
}

TEST(ParseTest, RegexAndRegexInGroup) {
  RunParseTest("/f{(o)(o)}", absl::InvalidArgumentError("expected '}'"));
}

TEST(ParseTest, RegexWithPrefix) {
  std::vector<Part> expected_parts = {
      Part(PartType::kRegex, /*name=*/"0", /*prefix=*/"/", "foo", /*suffix=*/"",
           Modifier::kNone),
  };
  RunParseTest("/(foo)", expected_parts);
}

TEST(ParseTest, RegexWithNameAndPrefix) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
      Part(PartType::kRegex, /*name=*/"bar", /*prefix=*/"/", "[^/]+?",
           /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/foo/:bar([^/]+?)", expected_parts);
}

TEST(ParseTest, RegexWithNameAndPrefixInGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo/", Modifier::kNone),
      Part(PartType::kRegex, /*name=*/"bar", /*prefix=*/"", "[^/]+?",
           /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/foo/{:bar([^/]+?)}", expected_parts);
}

TEST(ParseTest, RegexWithModifier) {
  std::vector<Part> expected_parts = {
      Part(PartType::kRegex, /*name=*/"0", /*prefix=*/"/", "foo",
           /*suffix=*/"", Modifier::kOptional),
  };
  RunParseTest("/(foo)?", expected_parts);
}

TEST(ParseTest, RegexLikeFullWildcard) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFullWildcard, /*name=*/"0", /*prefix=*/"/", /*value=*/"",
           /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/(.*)", expected_parts);
}

TEST(ParseTest, Wildcard) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFullWildcard, /*name=*/"0", /*prefix=*/"/", /*value=*/"",
           /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/*", expected_parts);
}

TEST(ParseTest, WildcardWithModifierStar) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFullWildcard, /*name=*/"0", /*prefix=*/"/", /*value=*/"",
           /*suffix=*/"", Modifier::kZeroOrMore),
  };
  RunParseTest("/**", expected_parts);
}

TEST(ParseTest, WildcardWithModifierPlus) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFullWildcard, /*name=*/"0", /*prefix=*/"/", /*value=*/"",
           /*suffix=*/"", Modifier::kOneOrMore),
  };
  RunParseTest("/*+", expected_parts);
}

TEST(ParseTest, WildcardWithModifierQuestion) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFullWildcard, /*name=*/"0", /*prefix=*/"/", /*value=*/"",
           /*suffix=*/"", Modifier::kOptional),
  };
  RunParseTest("/*?", expected_parts);
}

TEST(ParseTest, WildcardFollowingWildcardWithModifierStart) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFullWildcard, /*name=*/"0", /*prefix=*/"/", /*value=*/"",
           /*suffix=*/"", Modifier::kZeroOrMore),
      Part(PartType::kFullWildcard, /*name=*/"1", /*prefix=*/"", /*value=*/"",
           /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/***", expected_parts);
}

TEST(ParseTest, WildcardWithMultipleModifiersPlus) {
  RunParseTest("/**+", absl::InvalidArgumentError("expected end of pattern"));
}

TEST(ParseTest, WildcardWithMultipleModifiersQuestion) {
  RunParseTest("/**?", absl::InvalidArgumentError("expected end of pattern"));
}

TEST(ParseTest, WildcardInGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/f", Modifier::kNone),
      Part(PartType::kFullWildcard, /*name=*/"0", /*prefix=*/"", /*value=*/"",
           /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/f{*}", expected_parts);
}

TEST(ParseTest, WildcardWithPrefixAndSuffixInGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/", Modifier::kNone),
      Part(PartType::kFullWildcard, /*name=*/"0", /*prefix=*/"f", /*value=*/"",
           /*suffix=*/"o", Modifier::kNone),
  };
  RunParseTest("/{f*o}", expected_parts);
}

TEST(ParseTest, Name) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
      Part(PartType::kSegmentWildcard, /*name=*/"bar", /*prefix=*/"",
           /*value=*/"", /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/foo:bar", expected_parts);
}

TEST(ParseTest, NameStartsWithNumber) {
  RunParseTest("/foo/:0", absl::InvalidArgumentError("Missing parameter name"));
}

TEST(ParseTest, NameInGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
      Part(PartType::kSegmentWildcard, /*name=*/"bar", /*prefix=*/"",
           /*value=*/"", /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/foo{:bar}", expected_parts);
}

TEST(ParseTest, NameAndNameInGroup) {
  RunParseTest("/foo{:bar:baz}", absl::InvalidArgumentError("expected '}'"));
}

TEST(ParseTest, NameWithPrefixAndSuffixInGroup) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo/", Modifier::kNone),
      Part(PartType::kSegmentWildcard, /*name=*/"bar", /*prefix=*/"data_",
           /*value=*/"", /*suffix=*/".jpg", Modifier::kNone),
  };
  RunParseTest("/foo/{data_:bar.jpg}", expected_parts);
}

TEST(ParseTest, NameWithPrefix) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
      Part(PartType::kSegmentWildcard, /*name=*/"bar", /*prefix=*/"/",
           /*value=*/"", /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/foo/:bar", expected_parts);
}

TEST(ParseTest, NameWithEscapedPrefix) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo/", Modifier::kNone),
      Part(PartType::kSegmentWildcard, /*name=*/"bar", /*prefix=*/"",
           /*value=*/"", /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/foo\\/:bar", expected_parts);
}

TEST(ParseTest, NameWithCustomRegex) {
  std::vector<Part> expected_parts = {
      Part(PartType::kFixed, "/foo", Modifier::kNone),
      Part(PartType::kRegex, /*name=*/"bar", /*prefix=*/"", "[^/]+?",
           /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/foo:bar([^/]+?)", expected_parts);
}

TEST(ParseTest, NameWithModifier) {
  std::vector<Part> expected_parts = {
      Part(PartType::kSegmentWildcard, /*name=*/"foo", /*prefix=*/"/",
           /*value=*/"", /*suffix=*/"", Modifier::kOptional),
  };
  RunParseTest("/:foo?", expected_parts);
}

TEST(ParseTest, NameWithModifierStarAndWildcard) {
  std::vector<Part> expected_parts = {
      Part(PartType::kSegmentWildcard, /*name=*/"foo", /*prefix=*/"/",
           /*value=*/"", /*suffix=*/"", Modifier::kZeroOrMore),
      Part(PartType::kFullWildcard, /*name=*/"0", /*prefix=*/"",
           /*value=*/"", /*suffix=*/"", Modifier::kNone),
  };
  RunParseTest("/:foo**", expected_parts);
}

TEST(ParseTest, NameWithModifierStarAndModifierQuestion) {
  RunParseTest("/:foo*?",
               absl::InvalidArgumentError("expected end of pattern"));
}

TEST(ParseTest, NameWithModifierStarAndModifierPlus) {
  RunParseTest("/:foo*+",
               absl::InvalidArgumentError("expected end of pattern"));
}

TEST(ParseTest, DuplicateName) {
  RunParseTest("/:foo/:foo", absl::InvalidArgumentError("Duplicate"));
}

}  // namespace liburlpattern
