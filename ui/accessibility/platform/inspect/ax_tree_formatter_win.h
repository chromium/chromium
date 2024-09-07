// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_WIN_H_

#include <oleacc.h>
#include <wrl/client.h>

#include "base/component_export.h"
#include "third_party/iaccessible2/ia2_api_all.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

namespace ui {

class COMPONENT_EXPORT(AX_PLATFORM) AXTreeFormatterWin
    : public AXTreeFormatterBase {
 public:
  AXTreeFormatterWin();
  ~AXTreeFormatterWin() override;

  base::Value::Dict BuildTree(AXPlatformNodeDelegate* start) const override;
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

 protected:
  void AddDefaultFilters(
      std::vector<AXPropertyFilter>* property_filters) override;

 private:
  std::string EvaluateScript(
      Microsoft::WRL::ComPtr<IAccessible> root,
      const std::vector<AXScriptInstruction>& instructions,
      size_t start_index,
      size_t end_index) const;

  void RecursiveBuildTree(const Microsoft::WRL::ComPtr<IAccessible> node,
                          base::Value::Dict* dict,
                          LONG root_x,
                          LONG root_y) const;

  void AddProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                     base::Value::Dict* dict,
                     LONG root_x,
                     LONG root_y) const;
  void AddMSAAProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                         base::Value::Dict* dict,
                         LONG root_x,
                         LONG root_y) const;
  void AddSimpleDOMNodeProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                                  base::Value::Dict* dict) const;
  bool AddIA2Properties(const Microsoft::WRL::ComPtr<IAccessible>,
                        base::Value::Dict* dict) const;
  void AddIA2ActionProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                              base::Value::Dict* dict) const;
  void AddIA2HypertextProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                                 base::Value::Dict* dict) const;
  void AddIA2RelationProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                                base::Value::Dict* dict) const;
  void AddIA2RelationProperty(const Microsoft::WRL::ComPtr<IAccessibleRelation>,
                              base::Value::Dict* dict) const;
  void AddIA2TextProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                            base::Value::Dict* dict) const;
  void AddIA2TableProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                             base::Value::Dict* dict) const;
  void AddIA2TableCellProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                                 base::Value::Dict* dict) const;
  void AddIA2ValueProperties(const Microsoft::WRL::ComPtr<IAccessible>,
                             base::Value::Dict* dict) const;
  std::string ProcessTreeForOutput(
      const base::Value::Dict& node) const override;

  // Returns the root IAccessible object for the selector.
  Microsoft::WRL::ComPtr<IAccessible> FindAccessibleRoot(
      const AXTreeSelector& selector) const;

  // Returns a document accessible object for an active tab in a browser.
  Microsoft::WRL::ComPtr<IAccessible> FindActiveDocument(
      IAccessible* root) const;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_WIN_H_
