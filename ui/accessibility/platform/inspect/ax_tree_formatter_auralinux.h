// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_AURALINUX_H_

#include <atk/atk.h>
#include <atspi/atspi.h>

#include "base/component_export.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter_base.h"

namespace ui {

class AXPlatformNodeAuraLinux;

class COMPONENT_EXPORT(AX_PLATFORM) AXTreeFormatterAuraLinux
    : public AXTreeFormatterBase {
 public:
  AXTreeFormatterAuraLinux();
  ~AXTreeFormatterAuraLinux() override;

 private:
  std::string ProcessTreeForOutput(
      const base::Value::Dict& node) const override;

  base::Value::Dict BuildTree(AXPlatformNodeDelegate* root) const override;
  base::Value::Dict BuildTreeForSelector(
      const AXTreeSelector& selector) const override;

  base::Value::Dict BuildNode(AXPlatformNodeDelegate* node) const override;

  std::string EvaluateScript(const AXTreeSelector& selector,
                             const AXInspectScenario& scenario) const override;

  void RecursiveBuildTree(AtspiAccessible* node, base::Value::Dict* dict) const;
  void RecursiveBuildTree(AtkObject*, base::Value::Dict*) const;

  void AddProperties(AtkObject*, base::Value::Dict*) const;
  void AddProperties(AtspiAccessible*, base::Value::Dict*) const;

  void AddTextProperties(AtkObject* atk_object, base::Value::Dict* dict) const;
  void AddHypertextProperties(AtkObject* atk_object,
                              base::Value::Dict* dict) const;
  void AddActionProperties(AtkObject* atk_object,
                           base::Value::Dict* dict) const;
  void AddRelationProperties(AtkObject* atk_object,
                             base::Value::Dict* dict) const;
  void AddValueProperties(AtkObject* atk_object, base::Value::Dict* dict) const;
  void AddTableProperties(AtkObject* atk_object, base::Value::Dict* dict) const;
  void AddTableCellProperties(const AXPlatformNodeAuraLinux* node,
                              AtkObject* atk_object,
                              base::Value::Dict* dict) const;

  // Returns a string with the relation's name and the roles of the targets it
  // points to.
  static std::string ToString(AtkRelation* relation);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_AURALINUX_H_
