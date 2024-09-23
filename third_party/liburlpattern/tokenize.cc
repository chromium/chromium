// Copyright 2020 The Chromium Authors
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/tokenize.h"

#include <string_view>

#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/utf8.h"
#include "third_party/liburlpattern/utils.h"

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

class Tokenizer {
 public:
  Tokenizer(std::string_view pattern, TokenizePolicy policy)
      : pattern_(std::move(pattern)), policy_(policy) {
    token_list_.reserve(pattern_.size());
  }

  absl::StatusOr<std::vector<Token>> Tokenize() {
    while (index_ < pattern_.size()) {
      if (!status_.ok())
        return std::move(status_);

      if (!NextAt(index_)) {
        Error(absl::StrFormat("Invalid UTF-8 codepoint at index %d.", index_));
        continue;
      }
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
          Error(absl::StrFormat("Trailing escape character at index %d.",
                                index_));
          continue;
        }
        size_t escaped_i = next_index_;
        if (!Next()) {
          Error(absl::StrFormat("Invalid UTF-8 codepoint at index %d.",
                                next_index_));
          continue;
        }

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
          if (!status_.ok())
            return std::move(status_);
          if (!NextAt(pos)) {
            Error(absl::StrFormat("Invalid UTF-8 codepoint at index %d.", pos));
            continue;
          }
          if (!IsNameCodepoint(codepoint_, pos == name_start))
            break;
          pos = next_index_;
        }

        if (pos <= name_start) {
          Error(absl::StrFormat("Missing parameter name at index %d.", index_),
                name_start, index_);
          continue;
        }

        AddToken(TokenType::kName, pos, name_start);
        continue;
      }

      if (codepoint_ == '(') {
        size_t paren_nesting = 1;
        size_t j = next_index_;
        const size_t regex_start = next_index_;
        bool error = false;

        while (j < pattern_.size()) {
          if (!NextAt(j)) {
            Error(absl::StrFormat("Invalid UTF-8 codepoint at index %d.", j));
            error = true;
            break;
          }

          if (!IsASCII(codepoint_)) {
            Error(absl::StrFormat(
                      "Invalid non-ASCII character 0x%02x at index %d.",
                      codepoint_, j),
                  regex_start, index_);
            error = true;
            break;
          }
          if (j == regex_start && codepoint_ == '?') {
            Error(absl::StrFormat("Regex cannot start with '?' at index %d", j),
                  regex_start, index_);
            error = true;
            break;
          }

          // This escape processing only handles single character escapes since
          // we only need to understand escaped paren characters for our state
          // processing.  The escape `\` character is propagated to the embedded
          // regex expression.  This permits authors to write longer escape
          // sequences such as `\x22` since the later characters will be
          // propagated on subsequent loop iterations.
          if (codepoint_ == '\\') {
            if (j == (pattern_.size() - 1)) {
              Error(
                  absl::StrFormat("Trailing escape character at index %d.", j),
                  regex_start, index_);
              error = true;
              break;
            }
            size_t escaped_j = next_index_;
            if (!Next()) {
              Error(absl::StrFormat("Invalid UTF-8 codepoint at index %d.",
                                    next_index_));
              error = true;
              break;
            }
            if (!IsASCII(codepoint_)) {
              Error(absl::StrFormat(
                        "Invalid non-ASCII character 0x%02x at index %d.",
                        codepoint_, escaped_j),
                    regex_start, index_);
              error = true;
              break;
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
              Error(absl::StrFormat("Unbalanced regex at index %d.", j),
                    regex_start, index_);
              error = true;
              break;
            }
            size_t tmp_j = next_index_;
            if (!Next()) {
              Error(absl::StrFormat("Invalid UTF-8 codepoint at index %d.",
                                    next_index_));
              error = true;
              break;
            }
            // Require the the first character after an open paren is `?`.  This
            // permits assertions, named capture groups, and non-capturing
            // groups. It blocks, however, unnamed capture groups.
            if (codepoint_ != '?') {
              Error(absl::StrFormat(
                        "Unnamed capturing groups are not allowed at index %d.",
                        tmp_j),
                    regex_start, index_);
              error = true;
              break;
            }
            next_index_ = tmp_j;
          }

          j = next_index_;
        }

        if (error)
          continue;

        if (paren_nesting) {
          Error(absl::StrFormat("Unbalanced regex at index %d.", index_),
                regex_start, index_);
          continue;
        }

        const size_t regex_length = j - regex_start - 1;
        if (regex_length == 0) {
          Error(absl::StrFormat("Missing regex at index %d.", index_),
                regex_start, index_);
          continue;
        }

        AddToken(TokenType::kRegex, j, regex_start, regex_length);
        continue;
      }

      AddToken(TokenType::kChar);
    }

    if (!status_.ok())
      return std::move(status_);

    AddToken(TokenType::kEnd, index_, index_);

    return std::move(token_list_);
  }

 private:
  // Read the codepoint at `next_index_` in `pattern_` and store it in
  // `codepoint_`.  In addition, `next_index_` is updated to the codepoint to be
  // read next.  Returns true iff the codepoint was read successfully. On
  // success, `codepoint_` is non-negative.
  [[nodiscard]] bool Next() {
    U8_NEXT(pattern_.data(), next_index_, pattern_.size(), codepoint_);
    return codepoint_ >= 0;
  }

  // Read the codepoint at the specified `index` in `pattern_` and store it in
  // `codepoint_`.  In addition, `next_index_` is updated to the codepoint to be
  // read next.  Returns true iff the codepoint was read successfully. On
  // success, `codepoint_` is non-negative.
  [[nodiscard]] bool NextAt(size_t index) {
    next_index_ = index;
    return Next();
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

  void Error(std::string_view message, size_t next_pos, size_t value_pos) {
    if (policy_ == TokenizePolicy::kLenient)
      AddToken(TokenType::kInvalidChar, next_pos, value_pos);
    else
      status_ = absl::InvalidArgumentError(message);
  }

  void Error(std::string_view message) { Error(message, next_index_, index_); }

  const std::string_view pattern_;
  const TokenizePolicy policy_;
  std::vector<Token> token_list_;
  absl::Status status_;

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
      return "'{'";
    case TokenType::kClose:
      return "'}'";
    case TokenType::kRegex:
      return "regex group";
    case TokenType::kName:
      return "named group";
    case TokenType::kChar:
      return "character";
    case TokenType::kEscapedChar:
      return "escaped character";
    case TokenType::kOtherModifier:
      return "modifier";
    case TokenType::kAsterisk:
      return "asterisk";
    case TokenType::kEnd:
      return "end of pattern";
    case TokenType::kInvalidChar:
      return "invalid character";
  }
}

std::ostream& operator<<(std::ostream& o, Token token) {
  o << "{ type:" << static_cast<int>(token.type) << ", index:" << token.index
    << ", value:" << token.value << " }";
  return o;
}

// Split the input pattern into a list of tokens.
absl::StatusOr<std::vector<Token>> Tokenize(std::string_view pattern,
                                            TokenizePolicy policy) {
  Tokenizer tokenizer(std::move(pattern), policy);
  return tokenizer.Tokenize();
}

}  // namespace liburlpattern
