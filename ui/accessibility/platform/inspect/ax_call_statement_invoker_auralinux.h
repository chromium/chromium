// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_CALL_STATEMENT_INVOKER_AURALINUX_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_CALL_STATEMENT_INVOKER_AURALINUX_H_

#include <atspi/atspi.h>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/platform/inspect/ax_optional.h"
#include "ui/accessibility/platform/inspect/ax_tree_indexer_auralinux.h"

namespace ui {

class AXPropertyNode;

using Target =
    absl::variant<absl::monostate, std::string, int, const AtspiAccessible*>;

// Optional tri-state object.
using AXOptionalObject = AXOptional<Target>;

// Invokes a script instruction describing a call unit which represents
// a sequence of calls.
class COMPONENT_EXPORT(AX_PLATFORM) AXCallStatementInvokerAuraLinux final {
 public:
  // All calls are executed in the context of property nodes.
  // Note: both |indexer| and |storage| must outlive this object.
  AXCallStatementInvokerAuraLinux(const AXTreeIndexerAuraLinux* indexer,
                                  std::map<std::string, Target>* storage);

  // Invokes an attribute matching a property filter.
  AXOptionalObject Invoke(const AXPropertyNode& property_node,
                          bool no_object_parse = false) const;

  static std::string ToString(AXOptionalObject& optional);

 private:
  // Invokes a property node for a given target.
  AXOptionalObject InvokeFor(const Target target,
                             const AXPropertyNode& property_node) const;

  // Invokes a property node for a given AXElement.
  AXOptionalObject InvokeForAXElement(
      const AtspiAccessible* target,
      const AXPropertyNode& property_node) const;

  AXOptionalObject GetRole(const AtspiAccessible* target) const;

  AXOptionalObject GetName(const AtspiAccessible* target) const;

  AXOptionalObject GetDescription(const AtspiAccessible* target) const;

  AXOptionalObject GetParent(const AtspiAccessible* target) const;

  AXOptionalObject GetAttribute(const AtspiAccessible* target,
                                std::string attribute) const;

  AXOptionalObject HasState(const AtspiAccessible* target,
                            std::string state) const;

  AXOptionalObject GetRelation(const AtspiAccessible* target,
                               std::string relation) const;

  AXOptionalObject HasInterface(const AtspiAccessible* target,
                                std::string interface) const;

  bool IsAtspiAndNotNull(Target target) const;

  // Map between AXUIElement objects and their DOMIds/accessible tree
  // line numbers. Owned by the caller and outlives this object.
  const raw_ptr<const AXTreeIndexerAuraLinux> indexer_;

  // Variables storage. Owned by the caller and outlives this object.
  const raw_ptr<std::map<std::string, Target>> storage_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_CALL_STATEMENT_INVOKER_AURALINUX_H_
