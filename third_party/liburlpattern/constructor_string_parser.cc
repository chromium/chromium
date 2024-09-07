// Copyright 2021 The Chromium Authors
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/constructor_string_parser.h"

#include <string_view>
#include <vector>

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace liburlpattern {

ConstructorStringParser::ConstructorStringParser(
    std::string_view constructor_string)
    : input_(constructor_string) {}

absl::Status ConstructorStringParser::Parse(
    ProtocolCheckCallback protocol_matches_special_scheme) {
  ABSL_ASSERT(state_ == StringParseState::kInit);
  ABSL_ASSERT(token_index_ == 0u);

  auto tokenize_result = Tokenize(input_, TokenizePolicy::kLenient);
  if (!tokenize_result.ok()) {
    // This should not happen with kLenient mode, but we handle it anyway.
    return tokenize_result.status();
  }

  token_list_ = std::move(tokenize_result.value());

  // When constructing a pattern using structured input like
  // `new URLPattern({ pathname: 'foo' })` any missing components will be
  // defaulted to wildcards.
  //
  // Components which ordinarily appear "later" than those specified are instead
  // treated as wildcards, which avoids the need to explicitly wildcard each of
  // them. As a result, these values are not initialized to be empty until a
  // "later" component is seen.

  // Iterate through the list of tokens and update our state machine as we go.
  for (; token_index_ < token_list_.size(); token_index_ += token_increment_) {
    // Reset back to our default `token_increment_` value.
    token_increment_ = 1;

    // All states must respect the end of the token list.  The liburlpattern
    // tokenizer guarantees that the last token will have the type `kEnd`.
    if (token_list_[token_index_].type == TokenType::kEnd) {
      // If we failed to find a protocol terminator then we are still in
      // relative mode.  We now need to determine the first component of the
      // relative URL.
      if (state_ == StringParseState::kInit) {
        // Reset back to the start of the input string.
        Rewind();

        // If the string begins with `?` then its a relative search component.
        // If it starts with `#` then its a relative hash component.  Otherwise
        // its a relative pathname.
        //
        // In each case we initialize any components following the initial
        // component to be empty string.
        if (IsHashPrefix()) {
          ChangeState(StringParseState::kHash, Skip(1));
        } else if (IsSearchPrefix()) {
          ChangeState(StringParseState::kSearch, Skip(1));
        } else {
          ChangeState(StringParseState::kPathname, Skip(0));
        }
        continue;
      }

      // If we failed to find an `@`, then there is no username and password.
      // We should rewind and process the data as a hostname.
      else if (state_ == StringParseState::kAuthority) {
        RewindAndSetState(StringParseState::kHostname);
        continue;
      }

      ChangeState(StringParseState::kDone, Skip(0));
      break;
    }

    // In addition, all states must handle pattern groups.  We do not permit
    // a component to end in the middle of a pattern group.  Therefore we skip
    // past any tokens that are within `{` and `}`.  Note, the tokenizer
    // handles grouping `(` and `)` and `:foo` groups for us automatically, so
    // we don't need special code for them here.
    if (IsGroupOpen()) {
      group_depth_ += 1;
      continue;
    }

    if (group_depth_ > 0) {
      if (IsGroupClose()) {
        group_depth_ -= 1;
      } else {
        continue;
      }
    }

    switch (state_) {
      case StringParseState::kInit:
        if (IsProtocolSuffix()) {
          // Update the state to expect the start of an absolute URL.
          RewindAndSetState(StringParseState::kProtocol);
        }
        break;

      case StringParseState::kProtocol:
        // If we find the end of the protocol component...
        if (IsProtocolSuffix()) {
          absl::StatusOr<bool> protocol_check_result =
              protocol_matches_special_scheme(MakeComponentString());
          if (!protocol_check_result.ok()) {
            return protocol_check_result.status();
          }
          should_treat_as_standard_url_ = protocol_check_result.value();

          // By default we treat this as a "cannot-be-a-base-URL" or what chrome
          // calls a "path" URL.  In this case we go straight to the pathname
          // component.  The hostname and port are left with their default
          // empty string values.
          StringParseState next_state = StringParseState::kPathname;
          Skip skip = Skip(1);

          // If there are authority slashes, like `https://`, then
          // we must transition to the authority section of the URLPattern.
          if (NextIsAuthoritySlashes()) {
            next_state = StringParseState::kAuthority;
            skip = Skip(3);
          }

          // If there are no authority slashes, but the protocol is special
          // then we still go to the authority section as this is a "standard"
          // URL.  This differs from the above case since we don't need to skip
          // the extra slashes.
          else if (should_treat_as_standard_url_) {
            next_state = StringParseState::kAuthority;
          }

          ChangeState(next_state, skip);
        }
        break;

      case StringParseState::kAuthority:
        // Before going to the hostname state we must see if there is an
        // identity of the form:
        //
        //  <username>:<password>@<hostname>
        //
        // We check for this by looking for the `@` character.  The username
        // and password are themselves each optional, so the `:` may not be
        // present.  If we see the `@` we just go to the username state
        // and let it proceed until it hits either the password separator
        // or the `@` terminator.
        if (IsIdentityTerminator()) {
          RewindAndSetState(StringParseState::kUsername);
        }

        // Stop searching for the `@` character if we see the beginning
        // of the pathname, search, or hash components.
        else if (IsPathnameStart() || IsSearchPrefix() || IsHashPrefix()) {
          RewindAndSetState(StringParseState::kHostname);
        }
        break;

      case StringParseState::kUsername:
        // If we find a `:` then transition to the password component state.
        if (IsPasswordPrefix()) {
          ChangeState(StringParseState::kPassword, Skip(1));
        }

        // If we find a `@` then transition to the hostname component state.
        else if (IsIdentityTerminator()) {
          ChangeState(StringParseState::kHostname, Skip(1));
        }
        break;

      case StringParseState::kPassword:
        // If we find a `@` then transition to the hostname component state.
        if (IsIdentityTerminator()) {
          ChangeState(StringParseState::kHostname, Skip(1));
        }
        break;

      case StringParseState::kHostname:
        // Track whether we are inside ipv6 address brackets.
        if (IsIPv6Open()) {
          hostname_ipv6_bracket_depth_ += 1;
        } else if (IsIPv6Close()) {
          hostname_ipv6_bracket_depth_ -= 1;
        }

        // If we find a `:` then we transition to the port component state.
        // However, we ignore `:` when parsing an ipv6 address.
        else if (IsPortPrefix() && !hostname_ipv6_bracket_depth_) {
          ChangeState(StringParseState::kPort, Skip(1));
        }

        // If we find a `/` then we transition to the pathname component state.
        else if (IsPathnameStart()) {
          ChangeState(StringParseState::kPathname, Skip(0));
        }

        // If we find a `?` then we transition to the search component state.
        else if (IsSearchPrefix()) {
          ChangeState(StringParseState::kSearch, Skip(1));
        }

        // If we find a `#` then we transition to the hash component state.
        else if (IsHashPrefix()) {
          ChangeState(StringParseState::kHash, Skip(1));
        }
        break;

      case StringParseState::kPort:
        // If we find a `/` then we transition to the pathname component state.
        if (IsPathnameStart()) {
          ChangeState(StringParseState::kPathname, Skip(0));
        }
        // If we find a `?` then we transition to the search component state.
        else if (IsSearchPrefix()) {
          ChangeState(StringParseState::kSearch, Skip(1));
        }
        // If we find a `#` then we transition to the hash component state.
        else if (IsHashPrefix()) {
          ChangeState(StringParseState::kHash, Skip(1));
        }
        break;
      case StringParseState::kPathname:
        // If we find a `?` then we transition to the search component state.
        if (IsSearchPrefix()) {
          ChangeState(StringParseState::kSearch, Skip(1));
        }
        // If we find a `#` then we transition to the hash component state.
        else if (IsHashPrefix()) {
          ChangeState(StringParseState::kHash, Skip(1));
        }
        break;
      case StringParseState::kSearch:
        // If we find a `#` then we transition to the hash component state.
        if (IsHashPrefix()) {
          ChangeState(StringParseState::kHash, Skip(1));
        }
        break;
      case StringParseState::kHash:
        // Nothing to do here as we are just looking for the end.
        break;
      case StringParseState::kDone:
        ABSL_ASSERT(false);
        break;
    };
  }

  // Special case: if you specify a hostname, it is assumed that you want the
  // default port, if you didn't specify. This is ensures that
  // https://example.com/* does not match https://example.com:8443/, which is
  // another origin entirely.
  if (result_.hostname && !result_.port) {
    result_.port = "";
  }
  return absl::OkStatus();
}

void ConstructorStringParser::ChangeState(StringParseState new_state,
                                          Skip skip) {
  // First we convert the tokens between `component_start_` and `token_index_`
  // a component pattern string.  This is stored in the appropriate result
  // property based on the current `state_`.
  switch (state_) {
    case StringParseState::kInit:
      // No component to set when transitioning from this state.
      break;
    case StringParseState::kProtocol:
      result_.protocol = MakeComponentString();
      present_components_.protocol = true;
      break;
    case StringParseState::kAuthority:
      // No component to set when transitioning from this state.
      break;
    case StringParseState::kUsername:
      result_.username = MakeComponentString();
      present_components_.username = true;
      break;
    case StringParseState::kPassword:
      result_.password = MakeComponentString();
      present_components_.password = true;
      break;
    case StringParseState::kHostname:
      result_.hostname = MakeComponentString();
      present_components_.hostname = true;
      break;
    case StringParseState::kPort:
      result_.port = MakeComponentString();
      present_components_.port = true;
      break;
    case StringParseState::kPathname:
      result_.pathname = MakeComponentString();
      present_components_.pathname = true;
      break;
    case StringParseState::kSearch:
      result_.search = MakeComponentString();
      present_components_.search = true;
      break;
    case StringParseState::kHash:
      result_.hash = MakeComponentString();
      present_components_.hash = true;
      break;
    case StringParseState::kDone:
      ABSL_ASSERT(false);
      break;
  }

  if (state_ != StringParseState::kInit &&
      new_state != StringParseState::kDone) {
    // If a component was skipped but a later component is present, it gets its
    // default value, explicitly.
    //
    // This relies on the ordering of the states, which does correspond to the
    // order of components (aside from authority/username/password, which are
    // special).
    static_assert(StringParseState::kHostname < StringParseState::kPort);
    static_assert(StringParseState::kPort < StringParseState::kPathname);
    static_assert(StringParseState::kPathname < StringParseState::kSearch);
    static_assert(StringParseState::kSearch < StringParseState::kHash);
    if (state_ < StringParseState::kHostname &&
        new_state > StringParseState::kHostname && !result_.hostname) {
      result_.hostname = "";
    }
    if (state_ < StringParseState::kPort &&
        new_state > StringParseState::kPort && !result_.port) {
      result_.port = "";
    }
    if (state_ < StringParseState::kPathname &&
        new_state > StringParseState::kPathname && !result_.pathname) {
      result_.pathname = should_treat_as_standard_url_ ? "/" : "";
    }
    if (state_ < StringParseState::kSearch &&
        new_state > StringParseState::kSearch && !result_.search) {
      result_.search = "";
    }
  }

  ChangeStateWithoutSettingComponent(new_state, skip);
}

void ConstructorStringParser::ChangeStateWithoutSettingComponent(
    StringParseState new_state,
    Skip skip) {
  state_ = new_state;

  // Now update `component_start_` to point to the new component.  The `skip`
  // argument tells us how many tokens to ignore to get to the next start.
  component_start_ = token_index_ + skip;

  // Next, move the `token_index_` so that the top of the loop will begin
  // parsing the new component.  We adjust the `token_increment_` down to
  // zero as the skip value already takes into account moving to the start
  // of the next component.
  token_index_ += skip;
  token_increment_ = 0;
}

void ConstructorStringParser::Rewind() {
  token_index_ = component_start_;
  token_increment_ = 0;
}

void ConstructorStringParser::RewindAndSetState(StringParseState new_state) {
  Rewind();
  state_ = new_state;
}

const Token& ConstructorStringParser::SafeToken(size_t index) const {
  if (index < token_list_.size()) {
    return token_list_[index];
  }
  ABSL_ASSERT(!token_list_.empty());
  ABSL_ASSERT(token_list_.back().type == TokenType::kEnd);
  return token_list_.back();
}

bool ConstructorStringParser::IsNonSpecialPatternChar(size_t index,
                                                      const char* value) const {
  const Token& token = SafeToken(index);
  return token.value == value && (token.type == TokenType::kChar ||
                                  token.type == TokenType::kEscapedChar ||
                                  token.type == TokenType::kInvalidChar);
}

bool ConstructorStringParser::IsProtocolSuffix() const {
  return IsNonSpecialPatternChar(token_index_, ":");
}

bool ConstructorStringParser::NextIsAuthoritySlashes() const {
  return IsNonSpecialPatternChar(token_index_ + 1, "/") &&
         IsNonSpecialPatternChar(token_index_ + 2, "/");
}

bool ConstructorStringParser::IsIdentityTerminator() const {
  return IsNonSpecialPatternChar(token_index_, "@");
}

bool ConstructorStringParser::IsPasswordPrefix() const {
  return IsNonSpecialPatternChar(token_index_, ":");
}

bool ConstructorStringParser::IsPortPrefix() const {
  return IsNonSpecialPatternChar(token_index_, ":");
}

bool ConstructorStringParser::IsPathnameStart() const {
  return IsNonSpecialPatternChar(token_index_, "/");
}

bool ConstructorStringParser::IsSearchPrefix() const {
  if (IsNonSpecialPatternChar(token_index_, "?")) {
    return true;
  }

  if (token_list_[token_index_].value != "?") {
    return false;
  }

  // If we have a "?" that is not a normal character, then it must be an
  // optional group modifier.
  ABSL_ASSERT(SafeToken(token_index_).type == TokenType::kOtherModifier);

  // We have a `?` tokenized as a modifier.  We only want to treat this as
  // the search prefix if it would not normally be valid in a liburlpattern
  // string.  A modifier must follow a matching group.  Therefore we inspect
  // the preceding token to see if the `?` is immediately following a group
  // construct.
  //
  // So if the string is:
  //
  //  https://example.com/foo?bar
  //
  // Then we return true because the previous token is a `o` with type kChar.
  // For the string:
  //
  //  https://example.com/:name?bar
  //
  // Then we return false because the previous token is `:name` with type
  // kName.  If the developer intended this to be a search prefix then they
  // would need to escape like question mark like `:name\\?bar`.
  //
  // Note, if `token_index_` is zero the index will wrap around and
  // `SafeToken()` will return the kEnd token.  This will correctly return true
  // from this method as a pattern cannot normally begin with an unescaped `?`.
  const auto& previous_token = SafeToken(token_index_ - 1);
  return previous_token.type != TokenType::kName &&
         previous_token.type != TokenType::kRegex &&
         previous_token.type != TokenType::kClose &&
         previous_token.type != TokenType::kAsterisk;
}

bool ConstructorStringParser::IsHashPrefix() const {
  return IsNonSpecialPatternChar(token_index_, "#");
}

bool ConstructorStringParser::IsGroupOpen() const {
  return token_list_[token_index_].type == TokenType::kOpen;
}

bool ConstructorStringParser::IsGroupClose() const {
  return token_list_[token_index_].type == TokenType::kClose;
}

bool ConstructorStringParser::IsIPv6Open() const {
  return IsNonSpecialPatternChar(token_index_, "[");
}

bool ConstructorStringParser::IsIPv6Close() const {
  return IsNonSpecialPatternChar(token_index_, "]");
}

std::string_view ConstructorStringParser::MakeComponentString() const {
  ABSL_ASSERT(token_index_ < token_list_.size());
  const auto& token = token_list_[token_index_];

  size_t component_char_start = SafeToken(component_start_).index;

  ABSL_ASSERT(component_char_start <= input_.size());
  ABSL_ASSERT(token.index >= component_char_start);
  ABSL_ASSERT(token.index < input_.size() ||
              (token.index == input_.size() && token.type == TokenType::kEnd));
  return input_.substr(component_char_start,
                       token.index - component_char_start);
}

}  // namespace liburlpattern
