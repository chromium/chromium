// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef THIRD_PARTY_LIBURLPATTERN_LEXER_H_
#define THIRD_PARTY_LIBURLPATTERN_LEXER_H_

#include <vector>
#include "base/component_export.h"
#include "third_party/abseil-cpp/absl/status/statusor.h"
#include "third_party/abseil-cpp/absl/strings/string_view.h"

namespace liburlpattern {

enum class TokenType {
  // Open a scope with a '{'.
  kOpen,

  // Close a scope with a '}'.
  kClose,

  // A regular expression group like '(...)'.
  kRegex,

  // A named group like ':foo'.
  kName,

  // A single character.
  kChar,

  // The '\' escape character.
  kEscapedChar,

  // A '*', '+', or '?' modifier.
  kModifier,

  // The end of the token stream.
  kEnd,
};

// Simple structure representing a single lexical token.
struct COMPONENT_EXPORT(LIBURLPATTERN) Token {
  // Indicate the token type.
  TokenType type = TokenType::kEnd;

  // Index of the start of this token in the original pattern string.
  size_t index = 0;

  // The value of the token.  May be one or many characters depending on type.
  // May be null zero characters for the kEnd type.
  absl::string_view value;

  Token(TokenType t, size_t i, absl::string_view v)
      : type(t), index(i), value(v) {}
  Token() = default;
};

COMPONENT_EXPORT(LIBURLPATTERN)
inline bool operator==(const Token& lh, const Token& rh) {
  return lh.type == rh.type && lh.index == rh.index && lh.value == rh.value;
}

COMPONENT_EXPORT(LIBURLPATTERN)
inline bool operator!=(const Token& lh, const Token& rh) {
  return !(lh == rh);
}

COMPONENT_EXPORT(LIBURLPATTERN)
std::ostream& operator<<(std::ostream& o, Token token);

// Split the given input pattern string into a list of lexical tokens. Note,
// the generated Token objects simply reference positions within the input
// |pattern|.  The |pattern| must be kept alive as long as the Token objects.
COMPONENT_EXPORT(LIBURLPATTERN)
absl::StatusOr<std::vector<Token>> Tokenize(absl::string_view pattern);

}  // namespace liburlpattern

#endif  // THIRD_PARTY_LIBURLPATTERN_LEXER_H_
