// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/pattern.h"

#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/liburlpattern/utils.h"

namespace liburlpattern {

namespace {

void AppendModifier(Modifier modifier, std::string& append_target) {
  switch (modifier) {
    case Modifier::kNone:
      break;
    case Modifier::kOptional:
      append_target += '?';
      break;
    case Modifier::kZeroOrMore:
      append_target += '*';
      break;
    case Modifier::kOneOrMore:
      append_target += '+';
      break;
  }
}

size_t ModifierLength(Modifier modifier) {
  switch (modifier) {
    case Modifier::kNone:
      return 0;
    case Modifier::kOptional:
    case Modifier::kZeroOrMore:
    case Modifier::kOneOrMore:
      return 1;
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

Pattern::Pattern(std::vector<Part> part_list,
                 Options options,
                 std::string segment_wildcard_regex)
    : part_list_(std::move(part_list)),
      options_(std::move(options)),
      segment_wildcard_regex_(std::move(segment_wildcard_regex)) {}

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
        EscapeStringAndAppend(part.value, result);
      } else {
        result += "(?:";
        EscapeStringAndAppend(part.value, result);
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
    absl::string_view regex_value = part.value;
    if (part.type == PartType::kSegmentWildcard)
      regex_value = segment_wildcard_regex_;
    else if (part.type == PartType::kFullWildcard)
      regex_value = kFullWildcardRegex;

    // If there are no prefix or suffix values then we simply wrap the Part
    // regex value in a capturing group.  Any modifier is simply appended to
    // the end.  For example:
    //
    //  (<regex-value>)<modifier>
    //
    if (part.prefix.empty() && part.suffix.empty()) {
      absl::StrAppendFormat(&result, "(%s)", regex_value);
      AppendModifier(part.modifier, result);
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
      EscapeStringAndAppend(part.prefix, result);
      absl::StrAppendFormat(&result, "(%s)", regex_value);
      EscapeStringAndAppend(part.suffix, result);
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
    EscapeStringAndAppend(part.prefix, result);
    absl::StrAppendFormat(&result, "((?:%s)(?:", regex_value);
    EscapeStringAndAppend(part.suffix, result);
    EscapeStringAndAppend(part.prefix, result);
    absl::StrAppendFormat(&result, "(?:%s))*)", regex_value);
    EscapeStringAndAppend(part.suffix, result);
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
        result += EscapedLength(part.value);
      } else {
        // (?:<escaped-fixed-value>)<modifier>
        result += EscapedLength(part.value) + 4 + ModifierLength(part.modifier);
      }
      continue;
    }

    absl::string_view regex_value = part.value;
    if (part.type == PartType::kSegmentWildcard)
      regex_value = segment_wildcard_regex_;
    else if (part.type == PartType::kFullWildcard)
      regex_value = kFullWildcardRegex;

    if (part.prefix.empty() && part.suffix.empty()) {
      // (<regex-value>)<modifier>
      result += regex_value.size() + ModifierLength(part.modifier) + 2;
      continue;
    }

    size_t prefix_length = EscapedLength(part.prefix);
    size_t suffix_length = EscapedLength(part.suffix);

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
  EscapeStringAndAppend(options_.delimiter_list, append_target);
  append_target += "]";
}

size_t Pattern::DelimiterListLength() const {
  return EscapedLength(options_.delimiter_list) + 2;
}

void Pattern::AppendEndsWith(std::string& append_target) const {
  append_target += "[";
  EscapeStringAndAppend(options_.ends_with, append_target);
  append_target += "]|$";
}

size_t Pattern::EndsWithLength() const {
  return EscapedLength(options_.ends_with) + 4;
}

}  // namespace liburlpattern
