// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_tree_formatter.h"

#include <ostream>

#include "base/check.h"
#include "base/strings/pattern.h"
#include "base/values.h"

namespace ui {

// AXTreeFormatter

// static
bool AXTreeFormatter::MatchesPropertyFilters(
    const std::vector<AXPropertyFilter>& property_filters,
    const std::string& text,
    bool default_result) {
  bool allow = default_result;
  for (const auto& filter : property_filters) {
    // Either
    //   1) the line matches a filter pattern, for example, AXSubrole=* filter
    //      will match AXSubrole=AXTerm line or
    //   2) a property on the line is exactly equal to the filter pattern, for
    //      example, AXSubrole filter will match AXSubrole=AXTerm line.
    if (base::MatchPattern(text, filter.match_str) ||
        (filter.match_str.length() > 0 &&
         filter.match_str.find('=') == std::string::npos &&
         filter.match_str[filter.match_str.length() - 1] != '*' &&
         base::MatchPattern(text, filter.match_str + "=*"))) {
      switch (filter.type) {
        case AXPropertyFilter::ALLOW_EMPTY:
        case AXPropertyFilter::SCRIPT:
          allow = true;
          break;
        case AXPropertyFilter::ALLOW:
          allow = (!base::MatchPattern(text, "*=''"));
          break;
        case AXPropertyFilter::DENY:
          allow = false;
          break;
      }
    }
  }
  return allow;
}

// static
bool AXTreeFormatter::MatchesNodeFilters(
    const std::vector<AXNodeFilter>& node_filters,
    const base::Value::Dict& dict) {
  for (const auto& filter : node_filters) {
    if (filter.property == "*") {
      return true;
    }
    const std::string* value = dict.FindString(filter.property);
    if (value && base::MatchPattern(*value, filter.pattern)) {
      return true;
    }
  }
  return false;
}

}  // namespace ui
