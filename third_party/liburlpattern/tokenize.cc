// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/tokenize.h"

#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace liburlpattern {

namespace {

bool IsValidChar(char c) {
  // Characters should be valid ASCII code points:
  // https://infra.spec.whatwg.org/#ascii-code-point
  return c >= 0x00 && c <= 0x7f;
}

bool IsNameChar(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
         (c >= 'a' && c <= 'z') || c == '_';
}

}  // namespace

std::ostream& operator<<(std::ostream& o, Token token) {
  o << "{ type:" << static_cast<int>(token.type) << ", index:" << token.index
    << ", value:" << token.value << " }";
  return o;
}

// Split the input pattern into a list of tokens.  Originally translated to
// c++ from:
//
//  https://github.com/pillarjs/path-to-regexp/blob/125c43e6481f68cc771a5af22b914acdb8c5ba1f/src/index.ts#L4-L124
absl::StatusOr<std::vector<Token>> Tokenize(absl::string_view pattern) {
  // Verify that all characters are valid before parsing.  This simplifies the
  // following logic.
  for (size_t i = 0; i < pattern.size(); ++i) {
    if (!IsValidChar(pattern[i])) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid character 0x%02x at %d.", pattern[i], i));
    }
  }

  std::vector<Token> token_list;
  token_list.reserve(pattern.size());

  size_t i = 0;
  while (i < pattern.size()) {
    char c = pattern[i];
    if (c == '*' || c == '+' || c == '?') {
      token_list.emplace_back(TokenType::kModifier, i, pattern.substr(i, 1));
      i += 1;
      continue;
    }

    // Escape sequences always escape a single following character at the
    // level of the pattern.
    if (c == '\\') {
      if (i == (pattern.size() - 1)) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Trailing escape character at %d.", i));
      }
      token_list.emplace_back(TokenType::kEscapedChar, i,
                              pattern.substr(i + 1, 1));
      i += 2;
      continue;
    }

    if (c == '{') {
      token_list.emplace_back(TokenType::kOpen, i, pattern.substr(i, 1));
      i += 1;
      continue;
    }

    if (c == '}') {
      token_list.emplace_back(TokenType::kClose, i, pattern.substr(i, 1));
      i += 1;
      continue;
    }

    if (c == ':') {
      size_t j = i + 1;
      size_t name_start = j;
      while (j < pattern.size() && IsNameChar(pattern[j]))
        j += 1;

      if (j <= name_start) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Missing parameter name at %d.", i));
      }
      size_t name_length = j - name_start;

      token_list.emplace_back(TokenType::kName, i,
                              pattern.substr(name_start, name_length));
      i = j;
      continue;
    }

    if (c == '(') {
      size_t paren_nesting = 1;
      size_t j = i + 1;
      const size_t regex_start = j;

      while (j < pattern.size()) {
        if (j == regex_start && pattern[j] == '?') {
          return absl::InvalidArgumentError(
              absl::StrFormat("Regex cannot start with '?' at %d", j));
        }

        // This escape processing only handles single character escapes since
        // we only need to understand escaped paren characters for our state
        // processing.  The escape `\` character is propagated to the embedded
        // regex expression.  This permits authors to write longer escape
        // sequences such as `\x22` since the later characters will be
        // propagated on subsequent loop iterations.
        if (pattern[j] == '\\') {
          if (j == (pattern.size() - 1)) {
            return absl::InvalidArgumentError(
                absl::StrFormat("Trailing escape character at %d.", j));
          }
          j += 2;
          continue;
        }

        if (pattern[j] == ')') {
          paren_nesting -= 1;
          if (paren_nesting == 0) {
            j += 1;
            break;
          }
        } else if (pattern[j] == '(') {
          paren_nesting += 1;
          if (j == (pattern.size() - 1)) {
            return absl::InvalidArgumentError(
                absl::StrFormat("Unbalanced regex at %d.", i));
          }
          // Require the the first character after an open paren is `?`.  This
          // permits assertions, named capture groups, and non-capturing groups.
          // It blocks, however, unnamed capture groups.
          if (pattern[j + 1] != '?') {
            return absl::InvalidArgumentError(absl::StrFormat(
                "Unnamed capturing groups are not allowed at %d.", j));
          }
        }

        j += 1;
      }

      if (paren_nesting) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Unbalanced regex at %d.", i));
      }

      const size_t regex_length = j - regex_start - 1;
      if (regex_length == 0) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Missing regex at %d.", i));
      }

      token_list.emplace_back(TokenType::kRegex, i,
                              pattern.substr(regex_start, regex_length));
      i = j;
      continue;
    }

    token_list.emplace_back(TokenType::kChar, i, pattern.substr(i, 1));
    i += 1;
  }

  token_list.emplace_back(TokenType::kEnd, i, absl::string_view());

  return token_list;
}

}  // namespace liburlpattern
