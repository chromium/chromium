// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#ifndef THIRD_PARTY_LIBURLPATTERN_PATTERN_H_
#define THIRD_PARTY_LIBURLPATTERN_PATTERN_H_

#include <string>
#include <vector>
#include "base/component_export.h"

// NOTE: This code is a work-in-progress.  It is not ready for production use.

namespace liburlpattern {

enum class PartType {
  // A fixed, non-variable part of the pattern.  Consists of kChar and
  // kEscapedChar Tokens.
  kFixed,

  // A part with a custom regular expression.
  kRegex,

  // A part that matches any character to the next segment separator.
  kSegmentWildcard,

  // A part that matches any character to the end of the input string.
  kFullWildcard,
};

enum class Modifier {
  // No modifier.
  kNone,

  // The `?` modifier.
  kOptional,

  // The `*` modifier.
  kZeroOrMore,

  // The `+` modifier.
  kOneOrMore,
};

// A structure representing one part of a parsed Pattern.  A full Pattern
// consists of an ordered sequence of Part objects.
struct COMPONENT_EXPORT(LIBURLPATTERN) Part {
  // The type of the Part.
  const PartType type = PartType::kFixed;

  // The name of the Part.  Only kRegex, kSegmentWildcard, and kFullWildcard
  // parts may have a |name|.  kFixed parts must have an empty |name|.
  const std::string name;

  // A fixed string prefix that is expected before any regex or wildcard match.
  // kFixed parts must have an empty |prefix|.
  const std::string prefix;

  // The meaning of the |value| depends on the |type| of the Part.  For kFixed
  // parts the |value| contains the fixed string to match.  For kRegex parts
  // the |value| contains a regular expression to match.  The |value| is empty
  // for kSegmentWildcard and kFullWildcard parts since the |type| encodes what
  // to match.
  const std::string value;

  // A fixed string prefix that is expected after any regex or wildcard match.
  // kFixed parts must have an empty |suffix|.
  const std::string suffix;

  // A |modifier| indicating whether the Part is optional and/or repeated.  Any
  // Part type may have a |modifier|.
  const Modifier modifier = Modifier::kNone;

  Part(PartType type, std::string value, Modifier modifier);
  Part(PartType type,
       std::string name,
       std::string prefix,
       std::string value,
       std::string suffix,
       Modifier modifier);
  Part() = default;
};

COMPONENT_EXPORT(LIBURLPATTERN)
inline bool operator==(const Part& lh, const Part& rh) {
  return lh.name == rh.name && lh.prefix == rh.prefix && lh.value == rh.value &&
         lh.suffix == rh.suffix && lh.modifier == rh.modifier;
}

inline bool operator!=(const Part& lh, const Part& rh) {
  return !(lh == rh);
}

COMPONENT_EXPORT(LIBURLPATTERN)
std::ostream& operator<<(std::ostream& o, Part part);

// This class represents a successfully parsed pattern string.  It will contain
// an intermediate representation that can be used to generate either a regular
// expression string or to directly match against input strings.  Not all
// patterns are supported for direct matching.
class COMPONENT_EXPORT(LIBURLPATTERN) Pattern {
 public:
  explicit Pattern(std::vector<Part> part_list);

  const std::vector<Part>& PartList() const { return part_list_; }

 private:
  std::vector<Part> part_list_;
};

}  // namespace liburlpattern

#endif  // THIRD_PARTY_LIBURLPATTERN_PATTERN_H_
