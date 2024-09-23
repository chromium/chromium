// Copyright 2020 The Chromium Authors
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/parse.h"

#include <string_view>
#include <unordered_set>

#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/liburlpattern/tokenize.h"
#include "third_party/liburlpattern/utils.h"

// The following code is a translation from the path-to-regexp typescript at:
//
//  https://github.com/pillarjs/path-to-regexp/blob/125c43e6481f68cc771a5af22b914acdb8c5ba1f/src/index.ts#L126-L232

namespace liburlpattern {

namespace {

// Helper class that tracks the parser state.
class State {
 public:
  State(std::vector<Token> token_list,
        EncodeCallback encode_callback,
        Options options)
      : token_list_(std::move(token_list)),
        encode_callback_(std::move(encode_callback)),
        options_(std::move(options)),
        segment_wildcard_regex_(
            absl::StrFormat("[^%s]+?",
                            EscapeRegexpString(options_.delimiter_list))) {}

  // Return true if there are more tokens to process.
  bool HasMoreTokens() const { return index_ < token_list_.size(); }

  // Attempt to consume the next Token, but only if it matches the given
  // |type|.  Returns a pointer to the Token on success or nullptr on failure.
  const Token* TryConsume(TokenType type) {
    ABSL_ASSERT(index_ < token_list_.size());
    TokenType next_type = token_list_[index_].type;
    if (next_type != type)
      return nullptr;

    // The last token should always be kEnd.
    if ((index_ + 1) == token_list_.size())
      ABSL_ASSERT(token_list_[index_].type == TokenType::kEnd);

    return &(token_list_[index_++]);
  }

  // Consume the next Token requiring it to be the given |type|.  If this
  // is not possible then return an error.
  absl::StatusOr<const Token*> MustConsume(TokenType type) {
    ABSL_ASSERT(index_ < token_list_.size());
    if (const Token* token = TryConsume(type))
      return token;
    return absl::InvalidArgumentError(absl::StrFormat(
        "Unexpected %s '%s' at index %d, expected %s.",
        TokenTypeToString(token_list_[index_].type), token_list_[index_].value,
        token_list_[index_].index, TokenTypeToString(type)));
  }

  const Token* TryConsumeModifier() {
    const Token* result = TryConsume(TokenType::kOtherModifier);
    if (!result)
      result = TryConsume(TokenType::kAsterisk);
    return result;
  }

  // Consume as many sequential kChar and kEscapedChar Tokens as possible
  // appending them together into a single string value.
  std::string ConsumeText() {
    // Unfortunately we cannot use a view here and must copy into a new
    // string.  This is necessary to flatten escape sequences into
    // a single value with other characters.
    std::string result;
    const Token* token = nullptr;
    do {
      token = TryConsume(TokenType::kChar);
      if (!token)
        token = TryConsume(TokenType::kEscapedChar);
      if (token)
        result.append(token->value.data(), token->value.size());
    } while (token);
    return result;
  }

  // Append the given Token value to the pending fixed value.  This will
  // be converted to a kFixed Part when we reach the end of a run of
  // kChar and kEscapedChar tokens.
  void AppendToPendingFixedValue(std::string_view token_value) {
    pending_fixed_value_.append(token_value.data(), token_value.size());
  }

  // Convert the pending fixed value, if any, to a kFixed Part.  Has no effect
  // if there is no pending value.
  absl::Status MaybeAddPartFromPendingFixedValue() {
    if (pending_fixed_value_.empty())
      return absl::OkStatus();

    auto encoded_result = encode_callback_(std::move(pending_fixed_value_));
    if (!encoded_result.ok())
      return encoded_result.status();

    part_list_.emplace_back(PartType::kFixed, std::move(encoded_result.value()),
                            Modifier::kNone);
    pending_fixed_value_ = "";
    return absl::OkStatus();
  }

  // Add a Part for the given set of tokens.
  absl::Status AddPart(std::string prefix,
                       const Token* name_token,
                       const Token* regex_or_wildcard_token,
                       std::string suffix,
                       const Token* modifier_token) {
    // Convert the modifier Token into a Modifier enum value.
    Modifier modifier = Modifier::kNone;
    if (modifier_token) {
      ABSL_ASSERT(!modifier_token->value.empty());
      switch (modifier_token->value[0]) {
        case '?':
          modifier = Modifier::kOptional;
          break;
        case '*':
          modifier = Modifier::kZeroOrMore;
          break;
        case '+':
          modifier = Modifier::kOneOrMore;
          break;
        default:
          ABSL_ASSERT(false);
          break;
      }
    }

    // If this is a `{ ... }` grouping containing only fixed text, then
    // just add it to our pending value for now.  We want to collect as
    // much fixed text as possible in the buffer before commiting it to
    // a kFixed Part.
    if (!name_token && !regex_or_wildcard_token &&
        modifier == Modifier::kNone) {
      AppendToPendingFixedValue(prefix);
      return absl::OkStatus();
    }

    // We are about to add some kind of matching group Part to the list.
    // Before doing that make sure to flush any pending fixed test to a
    // kFixed Part.
    absl::Status status = MaybeAddPartFromPendingFixedValue();
    if (!status.ok())
      return status;

    // If there is no name, regex, or wildcard tokens then this is just a fixed
    // string grouping; e.g. "{foo}?".  The fixed string ends up in the prefix
    // value since it consumed the entire text of the grouping.  If the prefix
    // value is empty then its an empty "{}" group and we return without adding
    // any Part.
    if (!name_token && !regex_or_wildcard_token) {
      ABSL_ASSERT(suffix.empty());
      if (prefix.empty())
        return absl::OkStatus();
      auto result = encode_callback_(std::move(prefix));
      if (!result.ok())
        return result.status();
      part_list_.emplace_back(PartType::kFixed, *result, modifier);
      return absl::OkStatus();
    }

    // Determine the regex value.  If there is a |kRegex| Token, then this is
    // explicitly set by that Token.  If there is a wildcard token, then this
    // is set to the |kFullWildcardRegex| constant.  Otherwise a kName Token by
    // itself gets an implicit regex value that matches through to the end of
    // the segment. This is represented by the |segment_wildcard_regex_| value.
    std::string regex_value;
    if (!regex_or_wildcard_token)
      regex_value = segment_wildcard_regex_;
    else if (regex_or_wildcard_token->type == TokenType::kAsterisk)
      regex_value = kFullWildcardRegex;
    else
      regex_value = std::string(regex_or_wildcard_token->value);

    // Next determine the type of the Part.  This depends on the regex value
    // since we give certain values special treatment with their own type.
    // A |segment_wildcard_regex_| is mapped to the kSegmentWildcard type.  A
    // |kFullWildcardRegex| is mapped to the kFullWildcard type.  Otherwise
    // the Part gets the kRegex type.
    PartType type = PartType::kRegex;
    if (regex_value == segment_wildcard_regex_) {
      type = PartType::kSegmentWildcard;
      regex_value = "";
    } else if (regex_value == kFullWildcardRegex) {
      type = PartType::kFullWildcard;
      regex_value = "";
    }

    // Every kRegex, kSegmentWildcard, and kFullWildcard Part must have a
    // group name.  If there was a kName Token, then use the explicitly
    // set name.  Otherwise we generate a numeric based key for the name.
    std::string name;
    if (name_token)
      name = std::string(name_token->value);
    else if (regex_or_wildcard_token)
      name = GenerateKey();

    auto name_set_result = name_set_.insert(name);
    if (!name_set_result.second) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Duplicate group name '%s' at index %d.", name,
                          token_list_[index_].index));
    }

    auto prefix_result = encode_callback_(std::move(prefix));
    if (!prefix_result.ok())
      return prefix_result.status();

    auto suffix_result = encode_callback_(std::move(suffix));
    if (!suffix_result.ok())
      return suffix_result.status();

    // Finally add the part to the list.  We encode the prefix and suffix, but
    // must be careful not to encode the regex value since it can change the
    // meaning of the regular expression.
    part_list_.emplace_back(type, std::move(name), *prefix_result,
                            std::move(regex_value), *suffix_result, modifier);
    return absl::OkStatus();
  }

  Pattern TakeAsPattern() {
    return Pattern(std::move(part_list_), std::move(options_),
                   std::move(segment_wildcard_regex_));
  }

 private:
  // Generate a numeric key string to be used for groups that do not
  // have an explicit kName Token.
  std::string GenerateKey() { return absl::StrFormat("%d", next_key_++); }

  // The input list of Token objects to process.
  const std::vector<Token> token_list_;

  EncodeCallback encode_callback_;

  // The set of options used to parse and construct this Pattern.  This
  // controls the behavior of things like kSegmentWildcard parts, etc.
  Options options_;

  // The special regex value corresponding to the default regex value
  // given to a lone kName Token.  This is a variable since its value
  // is dependent on the |delimiter_list| passed to the constructor.
  const std::string segment_wildcard_regex_;

  // The output list of Pattern Part objects.
  std::vector<Part> part_list_;

  // Tracks which names have been seen before so we can error on duplicates.
  std::unordered_set<std::string> name_set_;

  // A buffer of kChar and kEscapedChar values that are pending the creation
  // of a kFixed Part.
  std::string pending_fixed_value_;

  // The index of the next Token in |token_list_|.
  size_t index_ = 0;

  // The next value to use when generating a numeric based name for Parts
  // without explicit kName Tokens.
  int next_key_ = 0;
};

}  // namespace

absl::StatusOr<Pattern> Parse(std::string_view pattern,
                              EncodeCallback encode_callback,
                              const Options& options) {
  auto result = Tokenize(pattern);
  if (!result.ok())
    return result.status();

  State state(std::move(result.value()), std::move(encode_callback), options);

  while (state.HasMoreTokens()) {
    // Look for the sequence: <prefix char><name><regex><modifier>
    // There could be from zero to all through of these tokens.  For
    // example:
    //  * "/:foo(bar)?" - all four tokens
    //  * "/" - just a char token
    //  * ":foo" - just a name token
    //  * "(bar)" - just a regex token
    //  * "/:foo" - char and name tokens
    //  * "/(bar)" - char and regex tokens
    //  * "/:foo?" - char, name, and modifier tokens
    //  * "/(bar)?" - char, regex, and modifier tokens
    const Token* char_token = state.TryConsume(TokenType::kChar);
    const Token* name_token = state.TryConsume(TokenType::kName);
    const Token* regex_or_wildcard_token = state.TryConsume(TokenType::kRegex);

    // If there is no name or regex token, then we may have a wildcard `*`
    // token in place of an unnamed regex token.  Each wildcard will be
    // treated as being equivalent to a "(.*)" regex token.  For example:
    //  * "/*" - equivalent to "/(.*)"
    //  * "/*?" - equivalent to "/(.*)?"
    if (!name_token && !regex_or_wildcard_token)
      regex_or_wildcard_token = state.TryConsume(TokenType::kAsterisk);

    // If there is a name, regex, or wildcard token then we need to add a
    // Pattern Part immediately.
    if (name_token || regex_or_wildcard_token) {
      // Determine if the char token is a valid prefix.  Only characters in the
      // configured prefix_list are automatically treated as prefixes.  A
      // kEscapedChar Token is never treated as a prefix.
      std::string_view prefix = char_token ? char_token->value : "";
      if (options.prefix_list.find(prefix.data(), /*pos=*/0, prefix.size()) ==
          std::string::npos) {
        // This is not a prefix character.  Add it to the buffered characters
        // to be added as a kFixed Part later.
        state.AppendToPendingFixedValue(prefix);
        prefix = std::string_view();
      }

      // If we have any buffered characters in a pending fixed value, then
      // convert them into a kFixed Part now.
      absl::Status status = state.MaybeAddPartFromPendingFixedValue();
      if (!status.ok())
        return status;

      // kName and kRegex tokens can optionally be followed by a modifier.
      const Token* modifier_token = state.TryConsumeModifier();

      // Add the Part for the name and regex/wildcard tokens.
      status = state.AddPart(std::string(prefix), name_token,
                             regex_or_wildcard_token,
                             /*suffix=*/"", modifier_token);
      if (!status.ok())
        return status;
      continue;
    }

    // There was neither a kRegex or kName token, so consider if we just have a
    // fixed string part.  A fixed string can consist of kChar or kEscapedChar
    // tokens.  These just get added to the buffered pending fixed value for
    // now. It will get converted to a kFixed Part later.
    const Token* fixed_token = char_token;
    if (!fixed_token)
      fixed_token = state.TryConsume(TokenType::kEscapedChar);
    if (fixed_token) {
      state.AppendToPendingFixedValue(fixed_token->value);
      continue;
    }

    // There was not a kChar or kEscapedChar token, so we no we are at the end
    // of any fixed string.  Do not yet convert the pending fixed value into
    // a kFixedPart, though.  Its possible there will be further fixed text in
    // a `{ ... }` group, etc.

    // Look for the sequence:
    //
    //  <open><char prefix><name><regex><char suffix><close><modifier>
    //
    // The open and close are required, but the other tokens are optional.
    // For example:
    //  * "{a:foo(.*)b}?" - all tokens present
    //  * "{:foo}?" - just name and modifier tokens
    //  * "{(.*)}?" - just regex and modifier tokens
    //  * "{ab}?" - just char and modifier tokens
    const Token* open_token = state.TryConsume(TokenType::kOpen);
    if (open_token) {
      std::string prefix = state.ConsumeText();
      const Token* name_token = state.TryConsume(TokenType::kName);
      const Token* regex_or_wildcard_token =
          state.TryConsume(TokenType::kRegex);

      // If there is no name or regex token, then we may have a wildcard `*`
      // token in place of an unnamed regex token.  Each wildcard will be
      // treated as being equivalent to a "(.*)" regex token.  For example,
      // "{a*b}" is equivalent to "{a(.*)b}".
      if (!name_token && !regex_or_wildcard_token)
        regex_or_wildcard_token = state.TryConsume(TokenType::kAsterisk);

      std::string suffix = state.ConsumeText();

      auto result = state.MustConsume(TokenType::kClose);
      if (!result.ok())
        return result.status();

      const Token* modifier_token = state.TryConsumeModifier();

      absl::Status status =
          state.AddPart(std::move(prefix), name_token, regex_or_wildcard_token,
                        std::move(suffix), modifier_token);
      if (!status.ok())
        return status;
      continue;
    }

    // We are about to end the pattern string, so flush any pending text to
    // a kFixed Part.
    absl::Status status = state.MaybeAddPartFromPendingFixedValue();
    if (!status.ok())
      return status;

    // We didn't find any tokens allowed by the syntax, so we should be
    // at the end of the token list.  If there is a syntax error, this
    // is where it will typically be caught.
    auto result = state.MustConsume(TokenType::kEnd);
    if (!result.ok())
      return result.status();
  }

  return state.TakeAsPattern();
}

}  // namespace liburlpattern
