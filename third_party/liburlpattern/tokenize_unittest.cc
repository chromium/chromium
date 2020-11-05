// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/tokenize.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace liburlpattern {

void RunTokenizerTest(absl::string_view pattern,
                      absl::StatusOr<std::vector<Token>> expected) {
  auto result = Tokenize(pattern);
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

TEST(TokenizerTest, Chars) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kChar, 1, "f"),
      Token(TokenType::kChar, 2, "o"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kEnd, 4, absl::string_view()),
  };
  RunTokenizerTest("/foo", expected_tokens);
}

TEST(TokenizerTest, CharsWithClosingParen) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kChar, 1, "f"),
      Token(TokenType::kChar, 2, "o"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, ")"),
      Token(TokenType::kEnd, 5, absl::string_view()),
  };
  RunTokenizerTest("/foo)", expected_tokens);
}

TEST(TokenizerTest, EscapedChar) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kEscapedChar, 1, "f"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kEnd, 5, absl::string_view()),
  };
  RunTokenizerTest("/\\foo", expected_tokens);
}

TEST(TokenizerTest, EscapedColon) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kEscapedChar, 1, ":"),
      Token(TokenType::kChar, 3, "f"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kChar, 5, "o"),
      Token(TokenType::kEnd, 6, absl::string_view()),
  };
  RunTokenizerTest("/\\:foo", expected_tokens);
}

TEST(TokenizerTest, EscapedParen) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kEscapedChar, 1, "("),
      Token(TokenType::kChar, 3, "f"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kChar, 5, "o"),
      Token(TokenType::kEscapedChar, 6, ")"),
      Token(TokenType::kEnd, 8, absl::string_view()),
  };
  RunTokenizerTest("/\\(foo\\)", expected_tokens);
}

TEST(TokenizerTest, EscapedCurlyBrace) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kEscapedChar, 1, "{"),
      Token(TokenType::kChar, 3, "f"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kChar, 5, "o"),
      Token(TokenType::kEscapedChar, 6, "}"),
      Token(TokenType::kEnd, 8, absl::string_view()),
  };
  RunTokenizerTest("/\\{foo\\}", expected_tokens);
}

TEST(TokenizerTest, EscapedCharAtEnd) {
  RunTokenizerTest("/foo\\",
                   absl::InvalidArgumentError("Trailing escape character"));
}

TEST(TokenizerTest, EscapedInvalidChar) {
  // Use a single byte invalid character since the escape only applies to the
  // next byte character.
  RunTokenizerTest("\\\xff", absl::InvalidArgumentError("Invalid character"));
}

TEST(TokenizerTest, Name) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kName, 0, "Foo_1"),
      Token(TokenType::kEnd, 6, absl::string_view()),
  };
  RunTokenizerTest(":Foo_1", expected_tokens);
}

TEST(TokenizerTest, NameWithZeroLength) {
  RunTokenizerTest("/:/foo",
                   absl::InvalidArgumentError("Missing parameter name"));
}

TEST(TokenizerTest, NameWithInvalidChar) {
  RunTokenizerTest("/:fooßar", absl::InvalidArgumentError("Invalid character"));
}

TEST(TokenizerTest, NameAndFileExtension) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kName, 0, "foo"),
      Token(TokenType::kChar, 4, "."),
      Token(TokenType::kChar, 5, "j"),
      Token(TokenType::kChar, 6, "p"),
      Token(TokenType::kChar, 7, "g"),
      Token(TokenType::kEnd, 8, absl::string_view()),
  };
  RunTokenizerTest(":foo.jpg", expected_tokens);
}

TEST(TokenizerTest, NameInPath) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kName, 1, "foo"),
      Token(TokenType::kChar, 5, "/"),
      Token(TokenType::kChar, 6, "b"),
      Token(TokenType::kChar, 7, "a"),
      Token(TokenType::kChar, 8, "r"),
      Token(TokenType::kEnd, 9, absl::string_view()),
  };
  RunTokenizerTest("/:foo/bar", expected_tokens);
}

TEST(TokenizerTest, Regex) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kRegex, 0, "foo"),
      Token(TokenType::kEnd, 5, absl::string_view()),
  };
  RunTokenizerTest("(foo)", expected_tokens);
}

TEST(TokenizerTest, RegexWithZeroLength) {
  RunTokenizerTest("()", absl::InvalidArgumentError("Missing regex"));
}

TEST(TokenizerTest, RegexWithInvalidChar) {
  RunTokenizerTest("(ßar)", absl::InvalidArgumentError("Invalid character"));
}

TEST(TokenizerTest, RegexWithoutClosingParen) {
  RunTokenizerTest("(foo", absl::InvalidArgumentError("Unbalanced regex"));
}

TEST(TokenizerTest, RegexWithNestedCapturingGroup) {
  RunTokenizerTest("(f(oo))", absl::InvalidArgumentError(
                                  "Unnamed capturing groups are not allowed"));
}

TEST(TokenizerTest, RegexWithNestedNamedCapturingGroup) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kRegex, 0, "f(?oo)"),
      Token(TokenType::kEnd, 8, absl::string_view()),
  };
  RunTokenizerTest("(f(?oo))", expected_tokens);
}

TEST(TokenizerTest, RegexWithNestedNonCapturingGroup) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kRegex, 0, "f(?:oo)"),
      Token(TokenType::kEnd, 9, absl::string_view()),
  };
  RunTokenizerTest("(f(?:oo))", expected_tokens);
}

TEST(TokenizerTest, RegexWithAssertion) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kRegex, 0, "f(?<y)x"),
      Token(TokenType::kEnd, 9, absl::string_view()),
  };
  RunTokenizerTest("(f(?<y)x)", expected_tokens);
}

TEST(TokenizerTest, RegexWithNestedUnbalancedGroup) {
  RunTokenizerTest("(f(?oo)", absl::InvalidArgumentError("Unbalanced regex"));
}

TEST(TokenizerTest, RegexWithTrailingParen) {
  RunTokenizerTest("(f(", absl::InvalidArgumentError("Unbalanced regex"));
}

TEST(TokenizerTest, RegexWithEscapedChar) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kRegex, 0, "f\\(oo"),
      Token(TokenType::kEnd, 7, absl::string_view()),
  };
  RunTokenizerTest("(f\\(oo)", expected_tokens);
}

TEST(TokenizerTest, RegexWithTrailingEscapedChar) {
  RunTokenizerTest("(foo\\",
                   absl::InvalidArgumentError("Trailing escape character"));
}

TEST(TokenizerTest, RegexWithEscapedInvalidChar) {
  // Use a single byte invalid character since the escape only applies to the
  // next byte character.
  RunTokenizerTest("(\\\xff)", absl::InvalidArgumentError("Invalid character"));
}

TEST(TokenizerTest, RegexWithLeadingQuestion) {
  RunTokenizerTest("(?foo)",
                   absl::InvalidArgumentError("Regex cannot start with '?'"));
}

TEST(TokenizerTest, RegexInPath) {
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
      Token(TokenType::kEnd, 13, absl::string_view()),
  };
  RunTokenizerTest("/foo/(.*)/bar", expected_tokens);
}

TEST(TokenizerTest, ModifierStar) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kOpen, 1, "{"),
      Token(TokenType::kChar, 2, "f"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kClose, 5, "}"),
      Token(TokenType::kModifier, 6, "*"),
      Token(TokenType::kEnd, 7, absl::string_view()),
  };
  RunTokenizerTest("/{foo}*", expected_tokens);
}

TEST(TokenizerTest, ModifierPlus) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kOpen, 1, "{"),
      Token(TokenType::kChar, 2, "f"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kClose, 5, "}"),
      Token(TokenType::kModifier, 6, "+"),
      Token(TokenType::kEnd, 7, absl::string_view()),
  };
  RunTokenizerTest("/{foo}+", expected_tokens);
}

TEST(TokenizerTest, ModifierQuestion) {
  std::vector<Token> expected_tokens = {
      Token(TokenType::kChar, 0, "/"),
      Token(TokenType::kOpen, 1, "{"),
      Token(TokenType::kChar, 2, "f"),
      Token(TokenType::kChar, 3, "o"),
      Token(TokenType::kChar, 4, "o"),
      Token(TokenType::kClose, 5, "}"),
      Token(TokenType::kModifier, 6, "?"),
      Token(TokenType::kEnd, 7, absl::string_view()),
  };
  RunTokenizerTest("/{foo}?", expected_tokens);
}

TEST(TokenizerTest, Everything) {
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
      Token(TokenType::kModifier, 21, "*"),
      Token(TokenType::kEnd, 22, absl::string_view()),
  };
  RunTokenizerTest("/\\foo/(a(?.*)){/:bar}*", expected_tokens);
}

}  // namespace liburlpattern
