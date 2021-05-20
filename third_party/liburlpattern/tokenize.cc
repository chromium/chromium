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

bool IsASCII(UChar32 c) {
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
    return u_hasBinaryProperty(c, UCHAR_ID_START) || c == '$' || c == '_';
  }
  return u_hasBinaryProperty(c, UCHAR_ID_CONTINUE) || c == '$' || c == '_' ||
         c == 0x200c || c == 0x200d;
}

class Tokenizer {
 public:
  explicit Tokenizer(absl::string_view pattern) : pattern_(pattern) {
    token_list_.reserve(pattern_.size());
  }

  absl::StatusOr<std::vector<Token>> Tokenize() {
    while (index_ < pattern_.size()) {
      NextAt(index_);
      if (codepoint_ == '*') {
        AddToken(TokenType::kAsterisk);
        continue;
      }

      if (codepoint_ == '+' || codepoint_ == '?') {
        AddToken(TokenType::kOtherModifier);
        continue;
      }

      // Escape sequences always escape a single following character at the
      // level of the pattern.
      if (codepoint_ == '\\') {
        if (index_ == (pattern_.size() - 1)) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Trailing escape character at %d.", index_));
        }
        size_t escaped_i = next_index_;
        Next();
        AddToken(TokenType::kEscapedChar, next_index_, escaped_i);
        continue;
      }

      if (codepoint_ == '{') {
        AddToken(TokenType::kOpen);
        continue;
      }

      if (codepoint_ == '}') {
        AddToken(TokenType::kClose);
        continue;
      }

      if (codepoint_ == ':') {
        size_t pos = next_index_;
        size_t name_start = pos;

        // Iterate over codepoints until we find the first non-name codepoint.
        while (pos < pattern_.size()) {
          NextAt(pos);
          if (!IsNameCodepoint(codepoint_, pos == name_start))
            break;
          pos = next_index_;
        }

        if (pos <= name_start) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Missing parameter name at %d.", index_));
        }

        AddToken(TokenType::kName, pos, name_start);
        continue;
      }

      if (codepoint_ == '(') {
        size_t paren_nesting = 1;
        size_t j = next_index_;
        const size_t regex_start = next_index_;

        while (j < pattern_.size()) {
          NextAt(j);

          if (!IsASCII(codepoint_)) {
            return absl::InvalidArgumentError(absl::StrFormat(
                "Invalid character 0x%02x at %d.", codepoint_, j));
          }
          if (j == regex_start && codepoint_ == '?') {
            return absl::InvalidArgumentError(
                absl::StrFormat("Regex cannot start with '?' at %d", j));
          }

          // This escape processing only handles single character escapes since
          // we only need to understand escaped paren characters for our state
          // processing.  The escape `\` character is propagated to the embedded
          // regex expression.  This permits authors to write longer escape
          // sequences such as `\x22` since the later characters will be
          // propagated on subsequent loop iterations.
          if (codepoint_ == '\\') {
            if (j == (pattern_.size() - 1)) {
              return absl::InvalidArgumentError(
                  absl::StrFormat("Trailing escape character at %d.", j));
            }
            size_t escaped_j = next_index_;
            Next();
            if (!IsASCII(codepoint_)) {
              return absl::InvalidArgumentError(absl::StrFormat(
                  "Invalid character 0x%02x at %d.", codepoint_, escaped_j));
            }
            j = next_index_;
            continue;
          }

          if (codepoint_ == ')') {
            paren_nesting -= 1;
            if (paren_nesting == 0) {
              j = next_index_;
              break;
            }
          } else if (codepoint_ == '(') {
            paren_nesting += 1;
            if (j == (pattern_.size() - 1)) {
              return absl::InvalidArgumentError(
                  absl::StrFormat("Unbalanced regex at %d.", j));
            }
            size_t tmp_j = next_index_;
            Next();
            // Require the the first character after an open paren is `?`.  This
            // permits assertions, named capture groups, and non-capturing
            // groups. It blocks, however, unnamed capture groups.
            if (codepoint_ != '?') {
              return absl::InvalidArgumentError(absl::StrFormat(
                  "Unnamed capturing groups are not allowed at %d.", tmp_j));
            }
            next_index_ = tmp_j;
          }

          j = next_index_;
        }

        if (paren_nesting) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Unbalanced regex at %d.", index_));
        }

        const size_t regex_length = j - regex_start - 1;
        if (regex_length == 0) {
          return absl::InvalidArgumentError(
              absl::StrFormat("Missing regex at %d.", index_));
        }

        AddToken(TokenType::kRegex, j, regex_start, regex_length);
        continue;
      }

      AddToken(TokenType::kChar);
    }

    AddToken(TokenType::kEnd, index_, index_);

    return std::move(token_list_);
  }

 private:
  // Read the codepoint at `next_index_` in `pattern_` and store it in
  // `codepoint_`.  In addition, `next_index_` is updated to the codepoint to be
  // read next.
  void Next() {
    U8_NEXT(pattern_.data(), next_index_, pattern_.size(), codepoint_);
  }

  // Read the codepoint at the specified `index` in `pattern_` and store it in
  // `codepoint_`.  In addition, `next_index_` is updated to the codepoint to be
  // read next.
  void NextAt(size_t index) {
    next_index_ = index;
    Next();
  }

  // Append a Token to our list of the given `type` and with a value consisting
  // of the codepoints in `pattern_` starting at `value_pos` with
  // `value_length`. Update `index_` to `next_pos` automatically.
  void AddToken(TokenType type,
                size_t next_pos,
                size_t value_pos,
                size_t value_length) {
    token_list_.emplace_back(type, index_,
                             pattern_.substr(value_pos, value_length));
    index_ = next_pos;
  }

  // Append a Token to our list of the given `type` and with a value consisting
  // of the codepoints in `pattern_` starting at `value_pos`.  The value length
  // is automatically computed as the difference between `next_pos` and
  // `value_pos`. Update `index_` to `next_pos` automatically.
  void AddToken(TokenType type, size_t next_pos, size_t value_pos) {
    AddToken(type, next_pos, value_pos, next_pos - value_pos);
  }

  // Append a Token to our list of the given `type` and with a value consisting
  // of the codepoints in `pattern_` starting at `index_`.  The value length
  // is automatically computed as the difference between `next_index_` and
  // `index_`. Update `index_` to `next_index_` automatically.
  void AddToken(TokenType type) { AddToken(type, next_index_, index_); }

  const absl::string_view pattern_;
  std::vector<Token> token_list_;

  // `index_` tracks our "current" byte index in the input string.  Typically
  // this will be updated every time we commit a token to `token_list_`.  It may
  // stay frozen in place if we have a sub-loop consuming a larger token like
  // a named group or regex group.
  size_t index_ = 0;

  // The `next_index_` member is used to find the next UTF8 codepoint in the
  // string.  This is used as both an input and output from the U8_NEXT()
  // function.  We keep this separate from `index_` because there are many
  // cases where we need to read ahead of the last token consumed.
  size_t next_index_ = 0;

  UChar32 codepoint_ = U_SENTINEL;
};

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
  Tokenizer tokenizer(pattern);
  return tokenizer.Tokenize();
}

}  // namespace liburlpattern
