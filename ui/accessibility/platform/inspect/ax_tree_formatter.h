// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/values.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class AXNode;
class AXScriptInstruction;
class AXTreeID;
class AXPlatformNodeDelegate;
class AXInspectScenario;

// A utility class for formatting platform-specific accessibility information,
// for use in testing, debugging, and developer tools.
// This is extended by a subclass for each platform where accessibility is
// implemented.
class COMPONENT_EXPORT(AX_PLATFORM) AXTreeFormatter {
 public:
  using AXTreeSelector = ui::AXTreeSelector;
  using AXPropertyFilter = ui::AXPropertyFilter;
  using AXNodeFilter = ui::AXNodeFilter;

  virtual ~AXTreeFormatter() = default;

  // Returns true if the given text matches |allow| property filters, or false
  // if matches |deny| filter. Returns default value if doesn't match any
  // property filters.
  static bool MatchesPropertyFilters(
      const std::vector<AXPropertyFilter>& property_filters,
      const std::string& text,
      bool default_result);

  // Check if the given dictionary matches any of the supplied AXNodeFilter(s).
  static bool MatchesNodeFilters(const std::vector<AXNodeFilter>& node_filters,
                                 const base::Value::Dict& dict);

  // Formats a given web content accessible tree.
  // |root| must be non-null and must be in web content.
  virtual std::string Format(AXPlatformNodeDelegate* root) const = 0;

  // Formats a given web node (i.e. without children).
  virtual std::string FormatNode(AXPlatformNodeDelegate* node) const = 0;

  // Similar to BuildTree, but generates a dictionary just for the current
  // web node (i.e. without children).
  virtual base::Value::Dict BuildNode(AXPlatformNodeDelegate* node) const = 0;

  // Build an accessibility tree for any window or pattern supplied by
  // the selector object.
  //
  // Returns a dictionary value with the accessibility tree populated.
  // The dictionary contains a key/value pair for each attribute of a node,
  // plus a "children" attribute containing a list of all child nodes.
  // {
  //   "AXName": "node",  /* actual attributes will vary by platform */
  //   "position": {  /* some attributes may be dictionaries */
  //     "x": 0,
  //     "y": 0
  //   },
  //   /* ... more attributes of |node| */
  //   "children": [ {  /* list of children created recursively */
  //     "AXName": "child node 1",
  //     /* ... more attributes */
  //     "children": [ ]
  //   }, {
  //     "AXName": "child name 2",
  //     /* ... more attributes */
  //     "children": [ ]
  //   } ]
  // }
  virtual base::Value::Dict BuildTreeForSelector(
      const AXTreeSelector&) const = 0;

  // Build an accessibility tree for an application with |node| as the root.
  virtual base::Value::Dict BuildTreeForNode(AXNode* node) const = 0;

  // Returns a string representing the internal tree represented by |tree_id|.
  virtual std::string DumpInternalAccessibilityTree(
      AXTreeID tree_id,
      const std::vector<AXPropertyFilter>& property_filters) = 0;

  // Dumps accessibility tree.
  virtual std::string FormatTree(const base::Value::Dict& tree_node) const = 0;

  // Evaluates script instructions for the window returned by the selector.
  virtual std::string EvaluateScript(
      const AXTreeSelector& selector,
      const AXInspectScenario& scenario) const = 0;

  // Evaluates script instructions between the given indices.
  virtual std::string EvaluateScript(
      AXPlatformNodeDelegate* root,
      const std::vector<AXScriptInstruction>& instructions,
      size_t start_index,
      size_t end_index) const = 0;

  // Property filter predefined sets.
  enum PropertyFilterSet {
    // Empty set.
    kFiltersEmptySet,

    // Default filters set, defined by a formatter.
    kFiltersDefaultSet,
  };

  // Set regular expression filters that apply to each property of every node
  // before it's output. If a default filter set is given, then filters defined
  // by the set are preappended to the given property filters.
  virtual void SetPropertyFilters(
      const std::vector<AXPropertyFilter>& property_filters,
      PropertyFilterSet default_filters_set = kFiltersEmptySet) = 0;

  // Set regular expression filters that apply to every node before output.
  virtual void SetNodeFilters(
      const std::vector<AXNodeFilter>& node_filters) = 0;

  // If true, the internal accessibility id of each node will be included
  // in its output.
  virtual void set_show_ids(bool show_ids) = 0;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_H_
