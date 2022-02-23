// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_MAC_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_MAC_H_

#include "ui/accessibility/platform/inspect/ax_call_statement_invoker_mac.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

namespace ui {

class AX_EXPORT AXTreeFormatterMac : public AXTreeFormatterBase {
 public:
  AXTreeFormatterMac();
  ~AXTreeFormatterMac() override;

  // AXTreeFormatter
  base::Value BuildTree(AXPlatformNodeDelegate* root) const override;
  base::Value BuildTreeForSelector(
      const AXTreeSelector& selector) const override;

  base::Value BuildNode(AXPlatformNodeDelegate* node) const override;

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
  base::Value BuildNode(const id node) const;

 protected:
  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;

 private:
  base::Value BuildTree(const id root) const;
  base::Value BuildTreeForAXUIElement(AXUIElementRef node) const;

  void RecursiveBuildTree(const id node,
                          const NSRect& root_rect,
                          const AXTreeIndexerMac* indexer,
                          base::Value* dict) const;

  void AddProperties(const id node,
                     const NSRect& root_rect,
                     const AXTreeIndexerMac* indexer,
                     base::Value* dict) const;

  // Invokes an attribute by a property node.
  AXOptionalNSObject InvokeAttributeFor(
      const NSAccessibilityElement* cocoa_node,
      const AXPropertyNode& property_node,
      const AXTreeIndexerMac* indexer) const;

  base::Value PopulateLocalPosition(const id node,
                                    const NSRect& root_rect) const;

  std::string ProcessTreeForOutput(
      const base::DictionaryValue& node) const override;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_MAC_H_
