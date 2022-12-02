// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_MAC_H_

#include "base/component_export.h"
#include "ui/accessibility/platform/inspect/ax_call_statement_invoker_mac.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) AXTreeFormatterMac
    : public AXTreeFormatterBase {
 public:
  AXTreeFormatterMac();
  ~AXTreeFormatterMac() override;

  // AXTreeFormatter
  base::Value::Dict BuildTree(AXPlatformNodeDelegate* root) const override;
  base::Value::Dict BuildTreeForSelector(
      const AXTreeSelector& selector) const override;

  base::Value::Dict BuildNode(AXPlatformNodeDelegate* node) const override;

  std::string EvaluateScript(const AXTreeSelector& selector,
                             const AXInspectScenario& scenario) const override;
  std::string EvaluateScript(
      AXPlatformNodeDelegate* root,
      const std::vector<AXScriptInstruction>& instructions,
      size_t start_index,
      size_t end_index) const override;
  std::string EvaluateScript(
      id platform_root,
      const std::vector<AXScriptInstruction>& instructions,
      size_t start_index,
      size_t end_index) const;

  // AXTreeFormatterMac
  base::Value::Dict BuildNode(const id node) const;

 protected:
  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;

 private:
  base::Value::Dict BuildTree(const id root) const;
  base::Value::Dict BuildTreeForAXUIElement(AXUIElementRef node) const;

  void RecursiveBuildTree(const AXElementWrapper& ax_element,
                          const NSRect& root_rect,
                          const AXTreeIndexerMac* indexer,
                          base::Value::Dict* dict) const;

  void AddProperties(const AXElementWrapper& ax_element,
                     const NSRect& root_rect,
                     const AXTreeIndexerMac* indexer,
                     base::Value::Dict* dict) const;

  // Invokes an attribute by a property node.
  AXOptionalNSObject InvokeAttributeFor(
      const NSAccessibilityElement* cocoa_node,
      const AXPropertyNode& property_node,
      const AXTreeIndexerMac* indexer) const;

  base::Value::Dict PopulateLocalPosition(const AXElementWrapper& ax_element,
                                          const NSRect& root_rect) const;

  std::string ProcessTreeForOutput(
      const base::Value::Dict& node) const override;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_MAC_H_
