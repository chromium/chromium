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
  std::string ProcessTreeForOutput(const base::DictValue& node) const override;

  base::DictValue BuildTree(AXPlatformNodeDelegate* root) const override;
  base::DictValue BuildTreeForSelector(
      const AXTreeSelector& selector) const override;

  base::DictValue BuildNode(AXPlatformNodeDelegate* node) const override;

  std::string EvaluateScript(const AXTreeSelector& selector,
                             const AXInspectScenario& scenario) const override;

  void RecursiveBuildTree(AtspiAccessible* node, base::DictValue* dict) const;
  void RecursiveBuildTree(AtkObject*, base::DictValue*) const;

  void AddProperties(AtkObject*, base::DictValue*) const;
  void AddProperties(AtspiAccessible*, base::DictValue*) const;

  void AddTextProperties(AtkObject* atk_object, base::DictValue* dict) const;
  void AddHypertextProperties(AtkObject* atk_object,
                              base::DictValue* dict) const;
  void AddActionProperties(AtkObject* atk_object, base::DictValue* dict) const;
  void AddRelationProperties(AtkObject* atk_object,
                             base::DictValue* dict) const;
  void AddValueProperties(AtkObject* atk_object, base::DictValue* dict) const;
  void AddTableProperties(AtkObject* atk_object, base::DictValue* dict) const;
  void AddTableCellProperties(const AXPlatformNodeAuraLinux* node,
                              AtkObject* atk_object,
                              base::DictValue* dict) const;

  // Returns a string with the relation's name and the roles of the targets it
  // points to.
  static std::string ToString(AtkRelation* relation);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_TREE_FORMATTER_AURALINUX_H_
