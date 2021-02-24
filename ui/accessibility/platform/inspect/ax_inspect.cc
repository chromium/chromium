// Copyright 2019 The Chromium Authors. All rights reserved.
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

AXPropertyFilter::AXPropertyFilter(const std::string& str, Type type)
    : match_str(str), type(type) {
  size_t index = str.find(';');
  if (index != std::string::npos) {
    filter_str = str.substr(0, index);
    if (index + 1 < str.length()) {
      match_str = str.substr(index + 1, std::string::npos);
    }
  }
  property_str = match_str.substr(0, match_str.find('='));
}

}  // namespace ui
