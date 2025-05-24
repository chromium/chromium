// Copyright 2020 The Chromium Authors
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef THIRD_PARTY_LIBURLPATTERN_PATTERN_H_
#define THIRD_PARTY_LIBURLPATTERN_PATTERN_H_

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/liburlpattern/options.h"
#include "third_party/liburlpattern/part.h"
#include "third_party/liburlpattern/utils.h"

namespace liburlpattern {

// This class represents a successfully parsed pattern string.  It will contain
// an intermediate representation that can be used to generate either a regular
// expression string or to directly match against input strings.  Not all
// patterns are supported for direct matching.
class COMPONENT_EXPORT(LIBURLPATTERN) Pattern {
 public:
  Pattern(std::vector<Part> part_list,
          Options options,
          std::string segment_wildcard_regex);

  // Generate a canonical string for the parsed pattern.  This may result
  // in a value different from the pattern string originally passed to
  // Parse().  For example, no-op syntax like `{bar}` will be simplified to
  // `bar`.  In addition, the generated string will include any changes mad
  // by EncodingCallback hooks.  Finally, regular expressions equivalent to
  // `*` and named group default matching will be simplified; e.g. `(.*)`
  // will become just `*`.
  std::string GeneratePatternString() const;

  // Generate an ECMA-262 regular expression string that is equivalent to this
  // pattern.  A vector of strings can be optionally passed to |name_list_out|
  // to be populated with the list of group names.  These correspond
  // sequentially to the regular expression capture groups.  Note, the
  // regular expression string does not currently used named capture groups
  // directly in order to match the upstream path-to-regexp behavior.
  std::string GenerateRegexString(
      std::vector<std::string>* name_list_out = nullptr) const;

  const std::vector<Part>& PartList() const { return part_list_; }

  // Returns true if the pattern has at least one kRegex part.
  bool HasRegexGroups() const;

  // Returns true if the pattern can match input strings using `DirectMatch()`.
  bool CanDirectMatch() const;

  // Attempts to match the pattern against the given `input` string directly
  // without using a regular expression.  Only some patterns support this
  // feature.  The caller must only call `DirectMatch()` if `CanDirectMatch()`
  // returns true.
  //
  // `DirectMatch()` returns true if the pattern matches `input` and false
  // otherwise.  If the `group_list_out` pointer is provided then the vector
  // is populated with name:value pairs for matched pattern groups.  If a
  // group had an optional modifier and it did not match any input characters
  // then its `group_list_out` value will be std::nullopt.
  bool DirectMatch(std::string_view input,
                   std::vector<std::pair<std::string_view,
                                         std::optional<std::string_view>>>*
                       group_list_out) const;

  // Generates a valid component string by filling non-fixed-text parts using
  // `groups` as a look-up table from names to substituting strings.  Every
  // used strings in `groups` will be encoded by the `callback` function.  This
  // function fails and returns an unexpected value when substitusion for names
  // in the pattern is not provided, any encoding attempt fail, or the pattern
  // contains unsupported syntax.  Currently, patterns only with SegmentWildcard
  // that have custom names are supported.  For example, `/foo/:bar` is
  // supported, but `/foo/*` is not.
  // TODO(crbug.com/414682820): Support more features.
  //
  // `groups` should not have overlaps in their names (first elements).  If
  // names have overlapped, the first appeared candidate will be used, but this
  // behavior is not guaranteed.
  base::expected<std::string, absl::Status> Generate(
      const std::unordered_map<std::string, std::string>& groups,
      EncodeCallback callback) const;

 private:
  // Compute the expected size of the string that will be returned by
  // GenerateRegexString().
  size_t RegexStringLength() const;

  // Utility method to help with generating the regex string and length.
  void AppendDelimiterList(std::string& append_target) const;
  size_t DelimiterListLength() const;
  void AppendEndsWith(std::string& append_target) const;
  size_t EndsWithLength() const;

  // Utility methods to help with direct matching.
  bool IsOnlyFullWildcard() const;
  bool IsOnlyFixedText() const;

  std::vector<Part> part_list_;
  Options options_;
  std::string segment_wildcard_regex_;
};

}  // namespace liburlpattern

#endif  // THIRD_PARTY_LIBURLPATTERN_PATTERN_H_
