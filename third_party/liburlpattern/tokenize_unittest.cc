// Copyright 2020 The Chromium Authors
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/tokenize.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace liburlpattern {

void RunTokenizeTest(std::string_view pattern,
                     absl::StatusOr<std::vector<Token>> expected,
                     TokenizePolicy policy = TokenizePolicy::kStrict) {
  auto result = Tokenize(pattern, policy);
  ASSERT_EQ(result.ok(), expected.ok()) << "lexer status for: " << pattern;
  if (!expected.ok()) {
    ASSERT_EQ(result.status().code(), expected.status().code())
        << "lexer status code for: " << pattern;
    EXPECT_NE(result.status().message().find(expected.status().message()),
              std::string::npos)
        << "lexer message '" << result.status().message()
        << "' does not contain '" << expected.status().message()
        << "' for: " << pattern;
    return;
  }
  const auto& expected_token_list = expected.value();
  const auto& token_list = result.value();
  EXPECT_EQ(token_list.size(), expected_token_list.size())
      << "lexer should produce expected number of tokens for: " << pattern;
  for (size_t i = 0; i < token_list.size() && i < expected_token_list.size();
       ++i) {
    EXPECT_EQ(token_list[i], expected_token_list[i])
        << "token at index " << i << " wrong for: " << pattern;
  }
}

TEST(TokenizeTest, EmptyPattern) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kEnd, 0, std::string_view()),
  };
  RunTokenizeTest("", expected_tokens);
}

TEST(TokenizeTest, Chars) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kChar, 1, "f"),
      Token(TokenType::kChar, 2, "o"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kEnd, 4, std::string_view()),
  };
  RunTokenizeTest("/foo", expected_tokens);
}

TEST(TokenizeTest, CharsWithClosingParen) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kChar, 1, "f"),
      Token(TokenType::kChar, 2, "o"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, ")"),
      Token(TokenType::kEnd, 5, std::string_view()),
  };
  RunTokenizeTest("/foo)", expected_tokens);
}

TEST(TokenizeTest, EscapedChar) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kEscapedChar, 1, "f"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kEnd, 5, std::string_view()),
  };
  RunTokenizeTest("/\\foo", expected_tokens);
}

TEST(TokenizeTest, EscapedColon) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kEscapedChar, 1, ":"),
      Token(TokenType::kChar, 3, "f"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kChar, 5, "o"),
      Token(TokenType::kEnd, 6, std::string_view()),
  };
  RunTokenizeTest("/\\:foo", expected_tokens);
}

TEST(TokenizeTest, EscapedParen) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kEscapedChar, 1, "("),
      Token(TokenType::kChar, 3, "f"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kChar, 5, "o"),
      Token(TokenType::kEscapedChar, 6, ")"),
      Token(TokenType::kEnd, 8, std::string_view()),
  };
  RunTokenizeTest("/\\(foo\\)", expected_tokens);
}

TEST(TokenizeTest, EscapedCurlyBrace) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kEscapedChar, 1, "{"),
      Token(TokenType::kChar, 3, "f"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kChar, 5, "o"),
      Token(TokenType::kEscapedChar, 6, "}"),
      Token(TokenType::kEnd, 8, std::string_view()),
  };
  RunTokenizeTest("/\\{foo\\}", expected_tokens);
}

TEST(TokenizeTest, EscapedCharAtEnd) {
  RunTokenizeTest("/foo\\",
                  absl::InvalidArgumentError("Trailing escape character"));
}

TEST(TokenizeTest, Name) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kName, 0, "Foo_1"),
      Token(TokenType::kEnd, 6, std::string_view()),
  };
  RunTokenizeTest(":Foo_1", expected_tokens);
}

TEST(TokenizeTest, NameWithZeroLength) {
  RunTokenizeTest("/:/foo",
                  absl::InvalidArgumentError("Missing parameter name"));
}

TEST(TokenizeTest, NameWithUnicodeChar) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kName, 1, "fooßar"),
      Token(TokenType::kEnd, 9, std::string_view()),
  };
  RunTokenizeTest("/:fooßar", expected_tokens);
}

TEST(TokenizeTest, NameWithSpaceFirstChar) {
  RunTokenizeTest("/: bad",
                  absl::InvalidArgumentError("Missing parameter name"));
}

TEST(TokenizeTest, NameWithDollarFirst) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kName, 1, "$foo"),
      Token(TokenType::kEnd, 6, std::string_view()),
  };
  RunTokenizeTest("/:$foo", expected_tokens);
}

TEST(TokenizeTest, NameWithDollarLater) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kName, 1, "foo$"),
      Token(TokenType::kEnd, 6, std::string_view()),
  };
  RunTokenizeTest("/:foo$", expected_tokens);
}

TEST(TokenizeTest, NameWithUnderscoreFirst) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kName, 1, "_foo"),
      Token(TokenType::kEnd, 6, std::string_view()),
  };
  RunTokenizeTest("/:_foo", expected_tokens);
}

TEST(TokenizeTest, NameWithUnderscoreLater) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kName, 1, "foo_"),
      Token(TokenType::kEnd, 6, std::string_view()),
  };
  RunTokenizeTest("/:foo_", expected_tokens);
}

TEST(TokenizeTest, NameFollowedByEscapedChar) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kName, 1, "foo"),
      Token(TokenType::kEscapedChar, 5, ":"),
      Token(TokenType::kEnd, 7, std::string_view()),
  };
  RunTokenizeTest("/:foo\\:", expected_tokens);
}

TEST(TokenizeTest, NameAndFileExtension) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kName, 0, "foo"),
      Token(TokenType::kChar, 4, "."),
      Token(TokenType::kChar, 5, "j"),
      Token(TokenType::kChar, 6, "p"),
      Token(TokenType::kChar, 7, "g"),
      Token(TokenType::kEnd, 8, std::string_view()),
  };
  RunTokenizeTest(":foo.jpg", expected_tokens);
}

TEST(TokenizeTest, NameInPath) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kName, 1, "foo"),
      Token(TokenType::kChar, 5, "/"),
      Token(TokenType::kChar, 6, "b"),
      Token(TokenType::kChar, 7, "a"),
      Token(TokenType::kChar, 8, "r"),
      Token(TokenType::kEnd, 9, std::string_view()),
  };
  RunTokenizeTest("/:foo/bar", expected_tokens);
}

TEST(TokenizeTest, Regex) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kRegex, 0, "foo"),
      Token(TokenType::kEnd, 5, std::string_view()),
  };
  RunTokenizeTest("(foo)", expected_tokens);
}

TEST(TokenizeTest, RegexWithZeroLength) {
  RunTokenizeTest("()", absl::InvalidArgumentError("Missing regex"));
}

TEST(TokenizeTest, RegexWithInvalidChar) {
  RunTokenizeTest("(ßar)",
                  absl::InvalidArgumentError("Invalid non-ASCII character"));
}

TEST(TokenizeTest, RegexWithoutClosingParen) {
  RunTokenizeTest("(foo", absl::InvalidArgumentError("Unbalanced regex"));
}

TEST(TokenizeTest, RegexWithNestedCapturingGroup) {
  RunTokenizeTest("(f(oo))", absl::InvalidArgumentError(
                                 "Unnamed capturing groups are not allowed"));
}

TEST(TokenizeTest, RegexWithNestedNamedCapturingGroup) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kRegex, 0, "f(?oo)"),
      Token(TokenType::kEnd, 8, std::string_view()),
  };
  RunTokenizeTest("(f(?oo))", expected_tokens);
}

TEST(TokenizeTest, RegexWithNestedNonCapturingGroup) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kRegex, 0, "f(?:oo)"),
      Token(TokenType::kEnd, 9, std::string_view()),
  };
  RunTokenizeTest("(f(?:oo))", expected_tokens);
}

TEST(TokenizeTest, RegexWithAssertion) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kRegex, 0, "f(?<y)x"),
      Token(TokenType::kEnd, 9, std::string_view()),
  };
  RunTokenizeTest("(f(?<y)x)", expected_tokens);
}

TEST(TokenizeTest, RegexWithNestedUnbalancedGroup) {
  RunTokenizeTest("(f(?oo)", absl::InvalidArgumentError("Unbalanced regex"));
}

TEST(TokenizeTest, RegexWithTrailingParen) {
  RunTokenizeTest("(f(", absl::InvalidArgumentError("Unbalanced regex"));
}

TEST(TokenizeTest, RegexWithEscapedChar) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kRegex, 0, "f\\(oo"),
      Token(TokenType::kEnd, 7, std::string_view()),
  };
  RunTokenizeTest("(f\\(oo)", expected_tokens);
}

TEST(TokenizeTest, RegexWithTrailingEscapedChar) {
  RunTokenizeTest("(foo\\",
                  absl::InvalidArgumentError("Trailing escape character"));
}

TEST(TokenizeTest, RegexWithEscapedInvalidChar) {
  // Use a valid UTF-8 sequence (encoding of U+00A2) that encodes a non-ASCII
  // character.
  RunTokenizeTest("(\\\xc2\xa2)",
                  absl::InvalidArgumentError("Invalid non-ASCII character"));
}

TEST(TokenizeTest, RegexWithLeadingQuestion) {
  RunTokenizeTest("(?foo)",
                  absl::InvalidArgumentError("Regex cannot start with '?'"));
}

TEST(TokenizeTest, RegexInPath) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kChar, 1, "f"),
      Token(TokenType::kChar, 2, "o"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "/"),
      Token(TokenType::kRegex, 5, ".*"),
      Token(TokenType::kChar, 9, "/"),
      Token(TokenType::kChar, 10, "b"),
      Token(TokenType::kChar, 11, "a"),
      Token(TokenType::kChar, 12, "r"),
      Token(TokenType::kEnd, 13, std::string_view()),
  };
  RunTokenizeTest("/foo/(.*)/bar", expected_tokens);
}

TEST(TokenizeTest, WildcardInPath) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kChar, 1, "f"),
      Token(TokenType::kChar, 2, "o"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "/"),
      Token(TokenType::kAsterisk, 5, "*"),
      Token(TokenType::kChar, 6, "/"),
      Token(TokenType::kChar, 7, "b"),
      Token(TokenType::kChar, 8, "a"),
      Token(TokenType::kChar, 9, "r"),
      Token(TokenType::kEnd, 10, std::string_view()),
  };
  RunTokenizeTest("/foo/*/bar", expected_tokens);
}

TEST(TokenizeTest, ModifierStar) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kOpen, 1, "{"),
      Token(TokenType::kChar, 2, "f"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kClose, 5, "}"),
      Token(TokenType::kAsterisk, 6, "*"),
      Token(TokenType::kEnd, 7, std::string_view()),
  };
  RunTokenizeTest("/{foo}*", expected_tokens);
}

TEST(TokenizeTest, ModifierPlus) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kOpen, 1, "{"),
      Token(TokenType::kChar, 2, "f"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kClose, 5, "}"),
      Token(TokenType::kOtherModifier, 6, "+"),
      Token(TokenType::kEnd, 7, std::string_view()),
  };
  RunTokenizeTest("/{foo}+", expected_tokens);
}

TEST(TokenizeTest, ModifierQuestion) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kOpen, 1, "{"),
      Token(TokenType::kChar, 2, "f"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kClose, 5, "}"),
      Token(TokenType::kOtherModifier, 6, "?"),
      Token(TokenType::kEnd, 7, std::string_view()),
  };
  RunTokenizeTest("/{foo}?", expected_tokens);
}

TEST(TokenizeTest, Everything) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kEscapedChar, 1, "f"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kChar, 5, "/"),
      Token(TokenType::kRegex, 6, "a(?.*)"),
      Token(TokenType::kOpen, 14, "{"),
      Token(TokenType::kChar, 15, "/"),
      Token(TokenType::kName, 16, "bar"),
      Token(TokenType::kClose, 20, "}"),
      Token(TokenType::kAsterisk, 21, "*"),
      Token(TokenType::kEnd, 22, std::string_view()),
  };
  RunTokenizeTest("/\\foo/(a(?.*)){/:bar}*", expected_tokens);
}

TEST(TokenizeTest, LenientPolicy) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "h"),
      Token(TokenType::kChar, 1, "t"),
      Token(TokenType::kChar, 2, "t"),
      Token(TokenType::kChar, 3, "p"),
      Token(TokenType::kInvalidChar, 4, ":"),
      Token(TokenType::kChar, 5, "/"),
      Token(TokenType::kChar, 6, "/"),
      Token(TokenType::kChar, 7, "a"),
      Token(TokenType::kChar, 8, "."),
      Token(TokenType::kChar, 9, "c"),
      Token(TokenType::kChar, 10, "o"),
      Token(TokenType::kChar, 11, "m"),
      Token(TokenType::kInvalidChar, 12, ":"),
      Token(TokenType::kChar, 13, "8"),
      Token(TokenType::kChar, 14, "0"),
      Token(TokenType::kChar, 15, "8"),
      Token(TokenType::kChar, 16, "0"),
      Token(TokenType::kChar, 17, "/"),
      Token(TokenType::kChar, 18, "f"),
      Token(TokenType::kChar, 19, "o"),
      Token(TokenType::kChar, 20, "o"),
      Token(TokenType::kOtherModifier, 21, "?"),
      Token(TokenType::kChar, 22, "b"),
      Token(TokenType::kChar, 23, "a"),
      Token(TokenType::kChar, 24, "r"),
      Token(TokenType::kChar, 25, "#"),
      Token(TokenType::kChar, 26, "b"),
      Token(TokenType::kChar, 27, "a"),
      Token(TokenType::kChar, 28, "z"),
      Token(TokenType::kEnd, 29, std::string_view()),
  };
  RunTokenizeTest("http://a.com:8080/foo?bar#baz", expected_tokens,
                  TokenizePolicy::kLenient);
}

TEST(TokenizeTest, LenientPolicyTrailingEscape) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "f"),
      Token(TokenType::kChar, 1, "o"),
      Token(TokenType::kChar, 2, "o"),
      Token(TokenType::kInvalidChar, 3, "\\"),
      Token(TokenType::kEnd, 4, std::string_view()),
  };
  RunTokenizeTest("foo\\", expected_tokens, TokenizePolicy::kLenient);
}

TEST(TokenizeTest, LenientPolicyRegexWithoutClose) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kInvalidChar, 0, "("),
      Token(TokenType::kChar, 1, "f"),
      Token(TokenType::kChar, 2, "o"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kEnd, 4, std::string_view()),
  };
  RunTokenizeTest("(foo", expected_tokens, TokenizePolicy::kLenient);
}

TEST(TokenizeTest, LenientPolicyRegexWithTrailingEscape) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kInvalidChar, 0, "("),
      Token(TokenType::kChar, 1, "f"),
      Token(TokenType::kChar, 2, "o"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kInvalidChar, 4, "\\"),
      Token(TokenType::kEnd, 5, std::string_view()),
  };
  RunTokenizeTest("(foo\\", expected_tokens, TokenizePolicy::kLenient);
}

TEST(TokenizeTest, LenientPolicyRegexWithCaptureGroup) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kInvalidChar, 0, "("),
      Token(TokenType::kChar, 1, "f"),
      Token(TokenType::kChar, 2, "o"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kRegex, 4, "bar"),
      Token(TokenType::kChar, 9, ")"),
      Token(TokenType::kEnd, 10, std::string_view()),
  };
  RunTokenizeTest("(foo(bar))", expected_tokens, TokenizePolicy::kLenient);
}

TEST(TokenizeTest, InvalidUtf8) {
  RunTokenizeTest("hello\xcdworld", absl::InvalidArgumentError(
                                        "Invalid UTF-8 codepoint at index 5."));
}

TEST(TokenizeTest, InvalidUtf8Escaped) {
  RunTokenizeTest(
      "hello\\\xcdworld",
      absl::InvalidArgumentError("Invalid UTF-8 codepoint at index 7."));
}

TEST(TokenizeTest, InvalidUtf8InName) {
  RunTokenizeTest(
      "/:foo:hello\xcdworld",
      absl::InvalidArgumentError("Invalid UTF-8 codepoint at index 11."));
}

TEST(TokenizeTest, InvalidUtf8InRegexGroup) {
  RunTokenizeTest("(foo\xcd)", absl::InvalidArgumentError(
                                   "Invalid UTF-8 codepoint at index 4."));
}

TEST(TokenizeTest, InvalidUtf8EscapedInRegexGroup) {
  RunTokenizeTest("(foo\\\xcd)", absl::InvalidArgumentError(
                                     "Invalid UTF-8 codepoint at index 6."));
}

TEST(TokenizeTest, InvalidUtf8InNestedRegexGroup) {
  RunTokenizeTest("(foo(\xcd))", absl::InvalidArgumentError(
                                     "Invalid UTF-8 codepoint at index 6."));
}

}  // namespace liburlpattern
