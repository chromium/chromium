// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LIBURLPATTERN_PART_H_
#define THIRD_PARTY_LIBURLPATTERN_PART_H_

#include <ostream>
#include <string>

#include "base/component_export.h"

namespace liburlpattern {

// Numeric values are set such that more restrictive values come last.  This
// is important for comparison routines in calling code, like URLPattern.
enum class PartType {
  // A part that matches any character to the end of the input string.
  kFullWildcard = 0,

  // A part that matches any character to the next segment separator.
  kSegmentWildcard = 1,

  // A part with a custom regular expression.
  kRegex = 2,

  // A fixed, non-variable part of the pattern.  Consists of kChar and
  // kEscapedChar Tokens.
  kFixed = 3,
};

// Numeric values are set such that more restrictive values come last.  This
// is important for comparison routines in calling code, like URLPattern.
enum class Modifier {
  // The `*` modifier.
  kZeroOrMore = 0,

  // The `?` modifier.
  kOptional = 1,

  // The `+` modifier.
  kOneOrMore = 2,

  // No modifier.
  kNone = 3,
};

// A structure representing one part of a parsed Pattern.  A full Pattern
// consists of an ordered sequence of Part objects.
struct COMPONENT_EXPORT(LIBURLPATTERN) Part {
  // The type of the Part.
  PartType type = PartType::kFixed;

  // The name of the Part.  Only kRegex, kSegmentWildcard, and kFullWildcard
  // parts may have a |name|.  kFixed parts must have an empty |name|.
  std::string name;

  // A fixed string prefix that is expected before any regex or wildcard match.
  // kFixed parts must have an empty |prefix|.
  std::string prefix;

  // The meaning of the |value| depends on the |type| of the Part.  For kFixed
  // parts the |value| contains the fixed string to match.  For kRegex parts
  // the |value| contains a regular expression to match.  The |value| is empty
  // for kSegmentWildcard and kFullWildcard parts since the |type| encodes what
  // to match.
  std::string value;

  // A fixed string prefix that is expected after any regex or wildcard match.
  // kFixed parts must have an empty |suffix|.
  std::string suffix;

  // A |modifier| indicating whether the Part is optional and/or repeated.  Any
  // Part type may have a |modifier|.
  Modifier modifier = Modifier::kNone;

  Part(PartType type, std::string value, Modifier modifier);
  Part(PartType type,
       std::string name,
       std::string prefix,
       std::string value,
       std::string suffix,
       Modifier modifier);
  Part() = default;

  // Returns true if the `name` member is a custom name; e.g. for a `:foo`
  // group.
  bool HasCustomName() const;
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
std::ostream& operator<<(std::ostream& o, const Part& part);

}  // namespace liburlpattern

#endif  // THIRD_PARTY_LIBURLPATTERN_PART_H_
