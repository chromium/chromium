// Copyright 2020 The Chromium Authors
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/pattern.h"

#include <optional>
#include <string_view>

#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/icu/source/common/unicode/utf8.h"
#include "third_party/liburlpattern/utils.h"

namespace liburlpattern {

namespace {

void AppendModifier(Modifier modifier, std::string& append_target) {
  switch (modifier) {
    case Modifier::kZeroOrMore:
      append_target += '*';
      break;
    case Modifier::kOptional:
      append_target += '?';
      break;
    case Modifier::kOneOrMore:
      append_target += '+';
      break;
    case Modifier::kNone:
      break;
  }
}

size_t ModifierLength(Modifier modifier) {
  switch (modifier) {
    case Modifier::kZeroOrMore:
    case Modifier::kOptional:
    case Modifier::kOneOrMore:
      return 1;
    case Modifier::kNone:
      return 0;
  }
}

}  // namespace

std::ostream& operator<<(std::ostream& o, Part part) {
  o << "{ type:" << static_cast<int>(part.type) << ", name:" << part.name
    << ", prefix:" << part.prefix << ", value:" << part.value
    << ", suffix:" << part.suffix
    << ", modifier:" << static_cast<int>(part.modifier) << " }";
  return o;
}

Part::Part(PartType t, std::string v, Modifier m)
    : type(t), value(std::move(v)), modifier(m) {
  ABSL_ASSERT(type == PartType::kFixed);
}

Part::Part(PartType t,
           std::string n,
           std::string p,
           std::string v,
           std::string s,
           Modifier m)
    : type(t),
      name(std::move(n)),
      prefix(std::move(p)),
      value(std::move(v)),
      suffix(std::move(s)),
      modifier(m) {
  ABSL_ASSERT(type != PartType::kFixed);
  ABSL_ASSERT(!name.empty());
  if (type == PartType::kFullWildcard || type == PartType::kSegmentWildcard)
    ABSL_ASSERT(value.empty());
}

bool Part::HasCustomName() const {
  // Determine if the part name was custom, like `:foo`, or an
  // automatically assigned numeric value.  Since custom group
  // names follow javascript identifier rules the first character
  // cannot be a digit, so that is all we need to check here.
  return !name.empty() && !std::isdigit(name[0]);
}

Pattern::Pattern(std::vector<Part> part_list,
                 Options options,
                 std::string segment_wildcard_regex)
    : part_list_(std::move(part_list)),
      options_(std::move(options)),
      segment_wildcard_regex_(std::move(segment_wildcard_regex)) {}

std::string Pattern::GeneratePatternString() const {
  std::string result;

  // Estimate the final length and reserve a reasonable sized string
  // buffer to avoid reallocations.
  size_t estimated_length = 0;
  for (const Part& part : part_list_) {
    // Add an arbitrary extra 3 per Part to account for braces, modifier, etc.
    estimated_length +=
        part.prefix.size() + part.value.size() + part.suffix.size() + 3;
  }
  result.reserve(estimated_length);

  for (size_t i = 0; i < part_list_.size(); ++i) {
    const Part& part = part_list_[i];

    if (part.type == PartType::kFixed) {
      // A simple fixed string part.
      if (part.modifier == Modifier::kNone) {
        EscapePatternStringAndAppend(part.value, result);
        continue;
      }

      // A fixed string, but with a modifier which requires a grouping.
      // For example, `{foo}?`.
      result += "{";
      EscapePatternStringAndAppend(part.value, result);
      result += "}";
      AppendModifier(part.modifier, result);
      continue;
    }

    bool custom_name = part.HasCustomName();

    // Determine if the part needs a grouping like `{ ... }`.  This is
    // necessary when the group:
    //
    // 1. is using a non-automatic prefix or any suffix.
    bool needs_grouping =
        !part.suffix.empty() ||
        (!part.prefix.empty() &&
         (part.prefix.size() != 1 ||
          options_.prefix_list.find(part.prefix[0]) == std::string::npos));

    // 2. followed by a matching group that may be expressed in a way that can
    //    be mistakenly interpreted as part of this matching group.  For
    //    example:
    //
    //    a. An `(...)` expression following a `:foo` group.  We want to
    //       output `{:foo}(...)` and not `:foo(...)`.
    //    b. A plaint text expression following a `:foo` group where the text
    //       could be mistakenly interpreted as part of the name.  We want to
    //       output `{:foo}bar` and not `:foobar`.
    const Part* next_part =
        (i + 1) < part_list_.size() ? &part_list_[i + 1] : nullptr;
    if (!needs_grouping && custom_name &&
        part.type == PartType::kSegmentWildcard &&
        part.modifier == Modifier::kNone && next_part &&
        next_part->prefix.empty() && next_part->suffix.empty()) {
      if (next_part->type == PartType::kFixed) {
        UChar32 codepoint = -1;
        U8_GET(reinterpret_cast<const uint8_t*>(next_part->value.data()), 0, 0,
               static_cast<int>(next_part->value.size()), codepoint);
        needs_grouping = IsNameCodepoint(codepoint, /*first_codepoint=*/false);
      } else {
        needs_grouping = !next_part->HasCustomName();
      }
    }

    // 3. preceded by a fixed text part that ends with an implicit prefix
    //    character (like `/`).  This occurs when the original pattern used
    //    an escape or grouping to prevent the implicit prefix; e.g.
    //    `\\/*` or `/{*}`.  In these cases we use a grouping to prevent the
    //    implicit prefix in the generated string.
    const Part* last_part = i > 0 ? &part_list_[i - 1] : nullptr;
    if (!needs_grouping && part.prefix.empty() && last_part &&
        last_part->type == PartType::kFixed) {
      needs_grouping = options_.prefix_list.find(last_part->value.back()) !=
                       std::string::npos;
    }

    // This is a full featured part.  We must generate a string that looks
    // like:
    //
    //  { <prefix> <value> <suffix> } <modifier>
    //
    // Where the { and } may not be needed.  The <value> will be a regexp,
    // named group, or wildcard.
    if (needs_grouping)
      result += "{";

    EscapePatternStringAndAppend(part.prefix, result);

    if (custom_name) {
      result += ":";
      result += part.name;
    }

    if (part.type == PartType::kRegex) {
      result += "(";
      result += part.value;
      result += ")";
    } else if (part.type == PartType::kSegmentWildcard) {
      // We only need to emit a regexp if a custom name was
      // not specified.  A custom name like `:foo` gets the
      // kSegmentWildcard type automatically.
      if (!custom_name) {
        result += "(";
        result += segment_wildcard_regex_;
        result += ")";
      }
    } else if (part.type == PartType::kFullWildcard) {
      // We can only use the `*` wildcard card if we meet a number
      // of conditions.  We must use an explicit `(.*)` group if:
      //
      // 1. A custom name was used; e.g. `:foo(.*)`.
      // 2. If the preceding group is a matching group without a modifier; e.g.
      //    `(foo)(.*)`.  In that case we cannot emit the `*` shorthand without
      //    it being mistakenly interpreted as the modifier for the previous
      //    group.
      // 3. The current group is not enclosed in a `{ }` grouping.
      // 4. The current group does not have an implicit prefix like `/`.
      if (!custom_name && (!last_part || last_part->type == PartType::kFixed ||
                           last_part->modifier != Modifier::kNone ||
                           needs_grouping || !part.prefix.empty())) {
        result += "*";
      } else {
        result += "(";
        result += kFullWildcardRegex;
        result += ")";
      }
    }

    // If the matching group is a simple `:foo` custom name with the default
    // segment wildcard, then we must check for a trailing suffix that could
    // be interpreted as a trailing part of the name itself.  In these cases
    // we must escape the beginning of the suffix in order to separate it
    // from the end of the custom name; e.g. `:foo\\bar` instead of `:foobar`.
    if (part.type == PartType::kSegmentWildcard && custom_name &&
        !part.suffix.empty()) {
      UChar32 codepoint = -1;
      U8_GET(reinterpret_cast<const uint8_t*>(part.suffix.data()), 0, 0,
             static_cast<int>(part.suffix.size()), codepoint);
      if (IsNameCodepoint(codepoint, /*first_codepoint=*/false)) {
        result += "\\";
      }
    }

    EscapePatternStringAndAppend(part.suffix, result);

    if (needs_grouping)
      result += "}";

    if (part.modifier != Modifier::kNone)
      AppendModifier(part.modifier, result);
  }

  return result;
}

// The following code is a translation from the path-to-regexp typescript at:
//
//  https://github.com/pillarjs/path-to-regexp/blob/125c43e6481f68cc771a5af22b914acdb8c5ba1f/src/index.ts#L532-L596
std::string Pattern::GenerateRegexString(
    std::vector<std::string>* name_list_out) const {
  std::string result;

  // This method mirrors the logic and structure of RegexStringLength().  If
  // one changes, so should the other.

  // Perform a full pass of the |part_list| to compute the length of the regex
  // string to avoid additional allocations.
  size_t expected_length = RegexStringLength();
  result.reserve(RegexStringLength());

  // Anchor to the start of the string if configured to in the options.
  if (options_.start)
    result += "^";

  // Iterate over each Part and append its equivalent value to the expression
  // string.
  for (const Part& part : part_list_) {
    // Handle kFixed Parts.  If there is a modifier we must wrap the escaped
    // value in a non-capturing group.  Otherwise we just append the escaped
    // value.  For example:
    //
    //  <escaped-fixed-value>
    //
    // Or:
    //
    //  (?:<escaped-fixed-value>)<modifier>
    //
    if (part.type == PartType::kFixed) {
      if (part.modifier == Modifier::kNone) {
        EscapeRegexpStringAndAppend(part.value, result);
      } else {
        result += "(?:";
        EscapeRegexpStringAndAppend(part.value, result);
        result += ")";
        AppendModifier(part.modifier, result);
      }
      continue;
    }

    // All remaining Part types must have a name.  Append it to the output
    // list if provided.
    ABSL_ASSERT(!part.name.empty());
    if (name_list_out)
      name_list_out->push_back(part.name);

    // Compute the Part regex value.  For kSegmentWildcard and kFullWildcard
    // types we must convert the type enum back to the defined regex value.
    std::string_view regex_value = part.value;
    if (part.type == PartType::kSegmentWildcard)
      regex_value = segment_wildcard_regex_;
    else if (part.type == PartType::kFullWildcard)
      regex_value = kFullWildcardRegex;

    // Handle the case where there is no prefix or suffix value.  This varies a
    // bit depending on the modifier.
    //
    // If there is no modifier or an optional modifier, then we simply wrap the
    // regex value in a capturing group:
    //
    //  (<regex-value>)<modifier>
    //
    // If there is a modifier, then we need to use a non-capturing group for the
    // regex value and an outer capturing group that includes the modifier as
    // well.  Like:
    //
    //  ((?:<regex-value>)<modifier>)
    if (part.prefix.empty() && part.suffix.empty()) {
      if (part.modifier == Modifier::kNone ||
          part.modifier == Modifier::kOptional) {
        absl::StrAppendFormat(&result, "(%s)", regex_value);
        AppendModifier(part.modifier, result);
      } else {
        absl::StrAppendFormat(&result, "((?:%s)", regex_value);
        AppendModifier(part.modifier, result);
        result += ")";
      }
      continue;
    }

    // Handle non-repeating regex Parts with a prefix and/or suffix.  The
    // capturing group again only contains the regex value.  This inner group
    // is compined with the prefix and/or suffix in an outer non-capturing
    // group.  Finally the modifier is applied to the entire outer group.
    // For example:
    //
    //  (?:<prefix>(<regex-value>)<suffix>)<modifier>
    //
    if (part.modifier == Modifier::kNone ||
        part.modifier == Modifier::kOptional) {
      result += "(?:";
      EscapeRegexpStringAndAppend(part.prefix, result);
      absl::StrAppendFormat(&result, "(%s)", regex_value);
      EscapeRegexpStringAndAppend(part.suffix, result);
      result += ")";
      AppendModifier(part.modifier, result);
      continue;
    }

    // Repeating Parts are dramatically more complicated.  We want to exclude
    // the initial prefix and the final suffix, but include them between any
    // repeated elements.  To achieve this we provide a separate initial
    // part that excludes the prefix.  Then the part is duplicated with the
    // prefix/suffix values included in an optional repeating element.  If
    // zero values are permitted then a final optional modifier may be added.
    // For example:
    //
    //  (?:<prefix>((?:<regex-value>)(?:<suffix><prefix>(?:<regex-value>))*)<suffix>)?
    //
    result += "(?:";
    EscapeRegexpStringAndAppend(part.prefix, result);
    absl::StrAppendFormat(&result, "((?:%s)(?:", regex_value);
    EscapeRegexpStringAndAppend(part.suffix, result);
    EscapeRegexpStringAndAppend(part.prefix, result);
    absl::StrAppendFormat(&result, "(?:%s))*)", regex_value);
    EscapeRegexpStringAndAppend(part.suffix, result);
    result += ")";
    if (part.modifier == Modifier::kZeroOrMore)
      result += "?";
  }

  // Should we anchor the pattern to the end of the input string?
  if (options_.end) {
    // In non-strict mode an optional delimiter character is always
    // permitted at the end of the string.  For example, if the pattern
    // is "/foo/bar" then it would match "/foo/bar/".
    //
    //  [<delimiter chars>]?
    //
    if (!options_.strict) {
      AppendDelimiterList(result);
      result += "?";
    }

    // The options ends_with value contains a list of characters that
    // may also signal the end of the pattern match.
    if (options_.ends_with.empty()) {
      // Simply anchor to the end of the input string.
      result += "$";
    } else {
      // Anchor to either a ends_with character or the end of the input
      // string.  This uses a lookahead assertion.
      //
      //  (?=[<ends_with chars>]|$)
      //
      result += "(?=";
      AppendEndsWith(result);
      result += ")";
    }

    return result;
  }

  // We are not anchored to the end of the input string.

  // Again, if not in strict mode we permit an optional trailing delimiter
  // character before anchoring to any ends_with characters with a lookahead
  // assertion.
  //
  //  (?:[<delimiter chars>](?=[<ends_with chars>]|$))?
  //
  if (!options_.strict) {
    result += "(?:";
    AppendDelimiterList(result);
    result += "(?=";
    AppendEndsWith(result);
    result += "))?";
  }

  // Further, if the pattern does not end with a trailing delimiter character
  // we also anchor to a delimiter character in our lookahead assertion.  So
  // a pattern "/foo/bar" would match "/foo/bar/baz", but not "/foo/barbaz".
  //
  //  (?=[<delimiter chars>]|[<ends_with chars>]|$)
  //
  bool end_delimited = false;
  if (!part_list_.empty()) {
    auto& last_part = part_list_.back();
    if (last_part.type == PartType::kFixed &&
        last_part.modifier == Modifier::kNone) {
      ABSL_ASSERT(!last_part.value.empty());
      end_delimited = options_.delimiter_list.find(last_part.value.back()) !=
                      std::string::npos;
    }
  }
  if (!end_delimited) {
    result += "(?=";
    AppendDelimiterList(result);
    result += "|";
    AppendEndsWith(result);
    result += ")";
  }

  ABSL_ASSERT(result.size() == expected_length);
  return result;
}

bool Pattern::HasRegexGroups() const {
  for (const Part& part : part_list_) {
    if (part.type == PartType::kRegex) {
      return true;
    }
  }
  return false;
}

bool Pattern::CanDirectMatch() const {
  // We currently only support direct matching with the options used by
  // URLPattern.
  if (!options_.start || !options_.end || !options_.strict ||
      !options_.sensitive) {
    return false;
  }

  return part_list_.empty() || IsOnlyFullWildcard() || IsOnlyFixedText();
}

bool Pattern::DirectMatch(
    std::string_view input,
    std::vector<
        std::pair<std::string_view, std::optional<std::string_view>>>*
        group_list_out) const {
  ABSL_ASSERT(CanDirectMatch());

  if (part_list_.empty())
    return input.empty();

  if (IsOnlyFullWildcard()) {
    if (group_list_out)
      group_list_out->emplace_back(part_list_[0].name, input);
    return true;
  }

  if (IsOnlyFixedText()) {
    return part_list_[0].value == input;
  }

  return false;
}

size_t Pattern::RegexStringLength() const {
  size_t result = 0;

  // This method mirrors the logic and structure of GenerateRegexString().  If
  // one changes, so should the other.  See GenerateRegexString() for an
  // explanation of the logic.

  if (options_.start) {
    // ^
    result += 1;
  }

  for (const Part& part : part_list_) {
    if (part.type == PartType::kFixed) {
      if (part.modifier == Modifier::kNone) {
        // <escaped-fixed-value>
        result += EscapedRegexpStringLength(part.value);
      } else {
        // (?:<escaped-fixed-value>)<modifier>
        result += EscapedRegexpStringLength(part.value) + 4 +
                  ModifierLength(part.modifier);
      }
      continue;
    }

    std::string_view regex_value = part.value;
    if (part.type == PartType::kSegmentWildcard)
      regex_value = segment_wildcard_regex_;
    else if (part.type == PartType::kFullWildcard)
      regex_value = kFullWildcardRegex;

    if (part.prefix.empty() && part.suffix.empty()) {
      // (<regex-value>)<modifier>
      result += regex_value.size() + ModifierLength(part.modifier) + 2;
      continue;
    }

    size_t prefix_length = EscapedRegexpStringLength(part.prefix);
    size_t suffix_length = EscapedRegexpStringLength(part.suffix);

    if (part.modifier == Modifier::kNone ||
        part.modifier == Modifier::kOptional) {
      // (?:<prefix>(<regex-value>)<suffix>)<modifier>
      result += prefix_length + regex_value.size() + suffix_length +
                ModifierLength(part.modifier) + 6;
      continue;
    }

    // (?:<prefix>((?:<regex-value>)(?:<suffix><prefix>(?:<regex-value>))*)<suffix>)?
    result += prefix_length + regex_value.size() + suffix_length +
              prefix_length + regex_value.size() + suffix_length + 19;
    if (part.modifier == Modifier::kZeroOrMore)
      result += 1;
  }

  if (options_.end) {
    if (!options_.strict) {
      // [<delimiter chars>]?
      result += DelimiterListLength() + 1;
    }

    if (options_.ends_with.empty()) {
      // $
      result += 1;
    } else {
      // (?=[<ends_with chars>]|$)
      result += EndsWithLength() + 4;
    }
  } else {
    bool end_delimited = false;
    if (!part_list_.empty()) {
      auto& last_part = part_list_.back();
      if (last_part.type == PartType::kFixed &&
          last_part.modifier == Modifier::kNone) {
        ABSL_ASSERT(!last_part.value.empty());
        end_delimited = options_.delimiter_list.find(last_part.value.back()) !=
                        std::string::npos;
      }
    }

    if (!options_.strict) {
      // (?:[<delimiter chars>](?=[<ends_with chars>]|$))?
      result += DelimiterListLength() + EndsWithLength() + 9;
    }

    if (!end_delimited) {
      // (?=[<delimiter chars>]|[<ends_with chars>]|$)
      result += DelimiterListLength() + EndsWithLength() + 5;
    }
  }

  return result;
}

void Pattern::AppendDelimiterList(std::string& append_target) const {
  append_target += "[";
  EscapeRegexpStringAndAppend(options_.delimiter_list, append_target);
  append_target += "]";
}

size_t Pattern::DelimiterListLength() const {
  return EscapedRegexpStringLength(options_.delimiter_list) + 2;
}

void Pattern::AppendEndsWith(std::string& append_target) const {
  append_target += "[";
  EscapeRegexpStringAndAppend(options_.ends_with, append_target);
  append_target += "]|$";
}

size_t Pattern::EndsWithLength() const {
  return EscapedRegexpStringLength(options_.ends_with) + 4;
}

bool Pattern::IsOnlyFullWildcard() const {
  if (part_list_.size() != 1)
    return false;
  auto& part = part_list_[0];
  // The modifier does not matter as an optional or repeated full wildcard
  // is functionally equivalent.
  return part.type == PartType::kFullWildcard && part.prefix.empty() &&
         part.suffix.empty();
}

bool Pattern::IsOnlyFixedText() const {
  if (part_list_.size() != 1)
    return false;
  auto& part = part_list_[0];
  bool result =
      part.type == PartType::kFixed && part.modifier == Modifier::kNone;
  if (result) {
    ABSL_ASSERT(part.prefix.empty());
    ABSL_ASSERT(part.suffix.empty());
  }
  return result;
}

}  // namespace liburlpattern
