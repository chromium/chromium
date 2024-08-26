// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_BASE_H_

#include <vector>

#include "base/component_export.h"
#include "base/values.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter.h"

namespace ui {

class AXPropertyNode;

// A utility class for formatting platform-specific accessibility information,
// for use in testing, debugging, and developer tools.
// This is extended by a subclass for each platform where accessibility is
// implemented.
class COMPONENT_EXPORT(AX_PLATFORM) AXTreeFormatterBase
    : public AXTreeFormatter {
 public:
  AXTreeFormatterBase();

  AXTreeFormatterBase(const AXTreeFormatterBase&) = delete;
  AXTreeFormatterBase& operator=(const AXTreeFormatterBase&) = delete;

  ~AXTreeFormatterBase() override;

  bool ShouldDumpNode(const AXPlatformNodeDelegate& node) const;
  bool ShouldDumpChildren(const AXPlatformNodeDelegate& node) const;

  // Build an accessibility tree for the current Chrome app.
  virtual base::Value::Dict BuildTree(AXPlatformNodeDelegate* root) const = 0;

  // AXTreeFormatter overrides.
  std::string Format(AXPlatformNodeDelegate* root) const override;
  std::string FormatNode(AXPlatformNodeDelegate* node) const override;
  std::string FormatTree(const base::Value::Dict& tree_node) const override;
  base::Value::Dict BuildTreeForNode(AXNode* root) const override;
  std::string EvaluateScript(const AXTreeSelector& selector,
                             const AXInspectScenario& scenario) const override;
  std::string EvaluateScript(
      AXPlatformNodeDelegate* root,
      const std::vector<AXScriptInstruction>& instructions,
      size_t start_index,
      size_t end_index) const override;
  void SetPropertyFilters(const std::vector<AXPropertyFilter>& property_filters,
                          PropertyFilterSet default_filters_set) override;
  void SetNodeFilters(const std::vector<AXNodeFilter>& node_filters) override;
  void set_show_ids(bool show_ids) override;
  std::string DumpInternalAccessibilityTree(
      AXTreeID tree_id,
      const std::vector<AXPropertyFilter>& property_filters) override;

 protected:
  static const char kChildrenDictAttr[];

  //
  // Overridden by platform subclasses.
  //

  // Appends default filters of the formatter.
  virtual void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters);

  // Returns property nodes complying to the line index filter for all
  // allow/allow_empty property filters.
  std::vector<AXPropertyNode> PropertyFilterNodesFor(
      const std::string& line_index) const;

  // Returns a list of script property nodes.
  std::vector<AXPropertyNode> ScriptPropertyNodes() const;

  // Return true if match-all filter is present.
  bool HasMatchAllPropertyFilter() const;

  // Process accessibility tree with filters for output.
  // Given a dictionary that contains a platform-specific dictionary
  // representing an accessibility tree, and utilizing property_filters_ and
  // node_filters_:
  // - Returns a filtered text view as one large string.
  // - Provides a filtered version of the dictionary in an out param,
  //   (only if the out param is provided).
  virtual std::string ProcessTreeForOutput(
      const base::Value::Dict& node) const = 0;

  //
  // Utility functions to be used by each platform.
  //

  std::string FormatCoordinates(const base::Value::Dict& dict,
                                const std::string& name,
                                const std::string& x_name,
                                const std::string& y_name) const;

  std::string FormatRectangle(const base::Value::Dict& dict,
                              const std::string& name,
                              const std::string& left_name,
                              const std::string& top_name,
                              const std::string& width_name,
                              const std::string& height_name) const;

  // Writes the given attribute string out to |line| if it matches the property
  // filters.
  // Returns false if the attribute was filtered out.
  bool WriteAttribute(bool include_by_default,
                      const std::string& attr,
                      std::string* line) const;
  void AddPropertyFilter(std::vector<AXPropertyFilter>* property_filters,
                         std::string filter,
                         AXPropertyFilter::Type type = AXPropertyFilter::ALLOW);
  bool show_ids() const { return show_ids_; }

  base::Value::Dict BuildNode(AXPlatformNodeDelegate* node) const override;

 private:
  void RecursiveFormatTree(const base::Value::Dict& tree_node,
                           std::string* contents,
                           int depth = 0) const;

  bool MatchesPropertyFilters(const std::string& text,
                              bool default_result) const;
  bool MatchesNodeFilters(const base::Value::Dict& dict) const;

  // Property filters used when formatting the accessibility tree as text.
  // Any property which matches a property filter will be skipped.
  std::vector<AXPropertyFilter> property_filters_;

  // Node filters used when formatting the accessibility tree as text.
  // Any node which matches a node wilder will be skipped, along with all its
  // children.
  std::vector<AXNodeFilter> node_filters_;

  // Whether or not node ids should be included in the dump.
  bool show_ids_ = false;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_BASE_H_
