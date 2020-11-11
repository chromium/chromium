// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_TREE_FORMATTER_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_TREE_FORMATTER_H_

#include "ui/accessibility/platform/inspect/inspect.h"

#include "ui/gfx/native_widget_types.h"

namespace base {
class Value;
class DictionaryValue;
}

namespace ui {

class AXPlatformNodeDelegate;

// A utility class for formatting platform-specific accessibility information,
// for use in testing, debugging, and developer tools.
// This is extended by a subclass for each platform where accessibility is
// implemented.
class AX_EXPORT AXTreeFormatter {
 public:
  using AXTreeSelector = ui::AXTreeSelector;
  using AXPropertyFilter = ui::AXPropertyFilter;
  using AXNodeFilter = ui::AXNodeFilter;

  virtual ~AXTreeFormatter() = default;

  // Appends default filters of the formatter.
  virtual void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) = 0;

  // Returns true if the given text matches |allow| property filters, or false
  // if matches |deny| filter. Returns default value if doesn't match any
  // property filters.
  static bool MatchesPropertyFilters(
      const std::vector<AXPropertyFilter>& property_filters,
      const std::string& text,
      bool default_result);

  // Check if the given dictionary matches any of the supplied AXNodeFilter(s).
  static bool MatchesNodeFilters(const std::vector<AXNodeFilter>& node_filters,
                                 const base::DictionaryValue& dict);

  // Build an accessibility tree for any window.
  virtual base::Value BuildTreeForWindow(
      gfx::AcceleratedWidget widget) const = 0;

  // Build an accessibility tree for an application with a name matching the
  // given pattern.
  virtual base::Value BuildTreeForSelector(const AXTreeSelector&) const = 0;

  // Returns a filtered accessibility tree using the current property and node
  // filters.
  virtual std::unique_ptr<base::DictionaryValue> FilterAccessibilityTree(
      const base::DictionaryValue& dict) = 0;

  // Dumps a BrowserAccessibility tree into a string.
  virtual void FormatAccessibilityTree(const base::DictionaryValue& tree_node,
                                       std::string* contents) = 0;

  // Test version of FormatAccessibilityTree().
  // |root| must be non-null and must be in web content.
  virtual void FormatAccessibilityTreeForTesting(AXPlatformNodeDelegate* root,
                                                 std::string* contents) = 0;

  // Set regular expression filters that apply to each property of every node
  // before it's output.
  virtual void SetPropertyFilters(
      const std::vector<AXPropertyFilter>& property_filters) = 0;

  // Set regular expression filters that apply to every node before output.
  virtual void SetNodeFilters(
      const std::vector<AXNodeFilter>& node_filters) = 0;

  // If true, the internal accessibility id of each node will be included
  // in its output.
  virtual void set_show_ids(bool show_ids) = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_TREE_FORMATTER_H_
