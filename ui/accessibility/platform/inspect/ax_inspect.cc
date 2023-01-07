// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_inspect.h"

namespace ui {

std::string AXTreeSelector::AppName() const {
  if (types & Chrome)
    return "Chrome";
  if (types & Chromium)
    return "Chromium";
  if (types & Edge)
    return "Edge";
  if (types & Firefox)
    return "Firefox";
  if (types & Safari)
    return "Safari";
  return "Unknown";
}

AXPropertyFilter::AXPropertyFilter(const AXPropertyFilter&) = default;

AXPropertyFilter& AXPropertyFilter::operator=(const AXPropertyFilter&) =
    default;

AXPropertyFilter::AXPropertyFilter(const std::string& str, Type type)
    : match_str(str), type(type) {
  size_t index = str.find(';');
  if (index != std::string::npos) {
    filter_str = str.substr(0, index);
    if (index + 1 < str.length()) {
      match_str = str.substr(index + 1, std::string::npos);
    }
  }

  // Extract a property string, which is stretched up to an optional value
  // string following '=' character. Note, a property can containing ':='
  // sequence, indicating a variable definition. Do not confuse it with a value
  // string start.
  index = match_str.rfind('=');
  if (index != 0 && index != std::string::npos && match_str[index - 1] == ':') {
    index = std::string::npos;
  }
  property_str = match_str.substr(0, index);
}

}  // namespace ui
