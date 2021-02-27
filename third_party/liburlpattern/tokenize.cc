// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/tokenize.h"

#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf8.h"

// The following code is a translation from the path-to-regexp typescript at:
//
//  https://github.com/pillarjs/path-to-regexp/blob/125c43e6481f68cc771a5af22b914acdb8c5ba1f/src/index.ts#L4-L124

namespace liburlpattern {

namespace {

bool IsASCII(char c) {
  // Characters should be valid ASCII code points:
  // https://infra.spec.whatwg.org/#ascii-code-point
  return c >= 0x00 && c <= 0x7f;
}

bool IsNameCodepoint(UChar32 c, bool first_codepoint) {
  // Require group names to follow the same character restrictions as
  // javascript identifiers.  This code originates from v8 at:
  //
  // https://source.chromium.org/chromium/chromium/src/+/master:v8/src/strings/char-predicates.cc;l=17-34;drc=be014256adea1552d4a044ef80616cdab6a7d549
  //
  // We deviate from js identifiers, however, in not support the backslash
  // character.  This is mainly used in js identifiers to allow escaped
  // unicode sequences to be written in ascii.  The js engine, however,
  // should take care of this long before we reach this level of code.  So
  // we don't need to handle it here.
  if (first_codepoint) {
    return u_hasBinaryProperty(c, UCHAR_ID_START) ||
           (c < 0x60 && (c == '$' || c == '_'));
  }
  return u_hasBinaryProperty(c, UCHAR_ID_CONTINUE) ||
         (c < 0x60 && (c == '$' || c == '_' || c == 0x200c || c == 0x200d));
}

}  // namespace

const char* TokenTypeToString(TokenType type) {
  switch (type) {
    case TokenType::kOpen:
      return "OPEN";
    case TokenType::kClose:
      return "CLOSE";
    case TokenType::kRegex:
      return "REGEX";
    case TokenType::kName:
      return "NAME";
    case TokenType::kChar:
      return "CHAR";
    case TokenType::kEscapedChar:
      return "ESCAPED_CHAR";
    case TokenType::kOtherModifier:
      return "OTHER_MODIFIER";
    case TokenType::kAsterisk:
      return "ASTERISK";
    case TokenType::kEnd:
      return "END";
  }
}

std::ostream& operator<<(std::ostream& o, Token token) {
  o << "{ type:" << static_cast<int>(token.type) << ", index:" << token.index
    << ", value:" << token.value << " }";
  return o;
}

// Split the input pattern into a list of tokens.
absl::StatusOr<std::vector<Token>> Tokenize(absl::string_view pattern) {
  // Verify that all characters are valid before parsing.  This simplifies the
  // following logic.
  for (size_t i = 0; i < pattern.size(); ++i) {
  }

  std::vector<Token> token_list;
  token_list.reserve(pattern.size());

  size_t i = 0;
  while (i < pattern.size()) {
    char c = pattern[i];
    if (c == '*') {
      token_list.emplace_back(TokenType::kAsterisk, i, pattern.substr(i, 1));
      i += 1;
      continue;
    }

    if (c == '+' || c == '?') {
      token_list.emplace_back(TokenType::kOtherModifier, i,
                              pattern.substr(i, 1));
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
      if (!IsASCII(pattern[i + 1])) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Invalid character 0x%02x at %d.", pattern[i], i));
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
      size_t pos = i + 1;
      size_t name_start = pos;
      while (pos < pattern.size()) {
        // Reads next unicode codepoint from utf8 sequence and automatically
        // updates the position index.  Since we're not sure this is a valid
        // codepoint yet, use |tmp_pos| for the position and only commit to
        // it once we check validity.
        size_t tmp_pos = pos;
        UChar32 codepoint;
        U8_NEXT(pattern.data(), tmp_pos, pattern.size(), codepoint);

        if (!IsNameCodepoint(codepoint, pos == name_start))
          break;

        pos = tmp_pos;
      }

      if (pos <= name_start) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Missing parameter name at %d.", i));
      }
      size_t name_length = pos - name_start;

      token_list.emplace_back(TokenType::kName, i,
                              pattern.substr(name_start, name_length));
      i = pos;
      continue;
    }

    if (c == '(') {
      size_t paren_nesting = 1;
      size_t j = i + 1;
      const size_t regex_start = j;

      while (j < pattern.size()) {
        if (!IsASCII(pattern[j])) {
          return absl::InvalidArgumentError(absl::StrFormat(
              "Invalid character 0x%02x at %d.", pattern[i], i));
        }
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
          if (!IsASCII(pattern[j + 1])) {
            return absl::InvalidArgumentError(absl::StrFormat(
                "Invalid character 0x%02x at %d.", pattern[i], i));
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

    if (!IsASCII(pattern[i])) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid character 0x%02x at %d.", pattern[i], i));
    }
    token_list.emplace_back(TokenType::kChar, i, pattern.substr(i, 1));
    i += 1;
  }

  token_list.emplace_back(TokenType::kEnd, i, absl::string_view());

  return token_list;
}

}  // namespace liburlpattern
