// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_INSPECT_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_INSPECT_H_

#include <string>

#include "ui/accessibility/ax_export.h"

namespace ui {

// Tree selector used to identify an accessible tree to traverse, it can be
// built by a pre-defined tree type like Chromium to indicate that Chromium
// browser tree should be traversed and/or by a string pattern which matches
// an accessible name of a root of some accessible subtree.
struct AX_EXPORT AXTreeSelector {
  enum Type {
    None = 0,
    ActiveTab = 1 << 0,
    Chrome = 1 << 1,
    Chromium = 1 << 2,
    Firefox = 1 << 3,
    Safari = 1 << 4,
  };
  int types{None};
  std::string pattern;

  AXTreeSelector() = default;
  AXTreeSelector(int types, const std::string& pattern)
      : types(types), pattern(pattern) {}

  bool empty() const { return types == None && pattern.empty(); }
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_INSPECT_H_
