// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_H_

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
    Edge = 1 << 3,
    Firefox = 1 << 4,
    Safari = 1 << 5,
  };
  int types{None};
  std::string pattern;

  AXTreeSelector() = default;
  AXTreeSelector(int types, const std::string& pattern)
      : types(types), pattern(pattern) {}

  bool empty() const { return types == None && pattern.empty(); }

  // Returns an application name for a type if the type specifies an
  // application.
  std::string AppName() const;
};

// A single property filter specification. Represents a parsed string of the
// filter_str;match_str format, where `filter_str` has
// :line_num_0,...:line_num_N format, `match_str` has format of
// property_str=value_str, value_str is optional. For example,
// AXSubrole=* or :1,:3;AXDOMClassList.
//
// Longer version: `filter_str` is a comma separated list of the line
// indexes from the output accessible tree, and serves to narrow down the
// property calls to the accessible object placed on those line indexes only;
// `match_str` is used to match properties by property name and value.
// For example, :1,:3;AXDOMClassList=*
// will query a AXDOMClassList attribute on accessible objects placed at 1st
// and 3rd lines in the output accessible tree.
// Also see
// DumpAccessibilityTestBase::ParseHtmlForExtraDirectives() for more
// information.
struct AX_EXPORT AXPropertyFilter {
  enum Type { ALLOW, ALLOW_EMPTY, DENY, SCRIPT };

  std::string match_str;
  std::string property_str;
  std::string filter_str;
  Type type;

  AXPropertyFilter(const std::string& str, Type type);
  AXPropertyFilter(const AXPropertyFilter&);
};

// A single node filter specification  which will exclude any node where the
// value of the named property matches the given pattern.
//
// This can be used to exclude nodes based on properties like role, for
// example to exclude all inlineTextBox nodes under blink we would use a
// NodeFilter of the form:
//   {property='internalRole', pattern='inlineTextBox'};
struct AX_EXPORT AXNodeFilter {
  std::string property;
  std::string pattern;

  AXNodeFilter(const std::string& property, const std::string& pattern)
      : property(property), pattern(pattern) {}
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_H_
