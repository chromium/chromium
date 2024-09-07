// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_CALL_STATEMENT_INVOKER_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_CALL_STATEMENT_INVOKER_WIN_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/accessibility/platform/inspect/ax_optional.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"
#include "ui/accessibility/platform/inspect/ax_target_win.h"
#include "ui/accessibility/platform/inspect/ax_tree_indexer_win.h"

namespace ui {

// Optional tri-state object.
using AXOptionalObject = AXOptional<AXTargetWin>;

// Invokes a script instruction describing a call unit which represents
// a sequence of calls.
class COMPONENT_EXPORT(AX_PLATFORM) AXCallStatementInvokerWin final {
 public:
  using Target = AXTargetWin;

  // All calls are executed in the context of property nodes.
  // Note: both |indexer| and |storage| must outlive this object.
  AXCallStatementInvokerWin(const AXTreeIndexerWin* indexer,
                            std::map<std::string, Target>* storage);

  // Invokes an attribute matching a property filter.
  AXOptionalObject Invoke(const AXPropertyNode& property_node) const;

  static std::string ToString(const AXOptionalObject& optional);

 private:
  // Invokes a property node for a given target.
  AXOptionalObject InvokeFor(const Target& target,
                             const AXPropertyNode& property_node) const;

  // Invokes a property node for a given AXElement.
  AXOptionalObject InvokeForAXElement(
      IAccessibleComPtr target,
      const AXPropertyNode& property_node) const;

  // Invoke for a given interface.
  AXOptionalObject InvokeForIA2(IA2ComPtr target,
                                const AXPropertyNode& property_node) const;
  AXOptionalObject InvokeForIA2Hypertext(
      IA2HypertextComPtr target,
      const AXPropertyNode& property_node) const;
  AXOptionalObject InvokeForIA2Table(IA2TableComPtr target,
                                     const AXPropertyNode& property_node) const;
  AXOptionalObject InvokeForIA2TableCell(
      IA2TableCellComPtr target,
      const AXPropertyNode& property_node) const;
  AXOptionalObject InvokeForIA2TextSelectionContainer(
      IA2TextSelectionContainerComPtr target,
      const AXPropertyNode& property_node) const;
  AXOptionalObject InvokeForIA2Text(IA2TextComPtr target,
                                    const AXPropertyNode& property_node) const;
  AXOptionalObject InvokeForIA2Value(IA2ValueComPtr target,
                                     const AXPropertyNode& property_node) const;

  // IUnknown functionality.
  AXOptionalObject QueryInterface(const IAccessibleComPtr target,
                                  std::string interface_name) const;

  // IAccessible functionality.
  AXOptionalObject GetRole(IAccessibleComPtr target) const;
  AXOptionalObject GetName(const IAccessibleComPtr target) const;
  AXOptionalObject GetDescription(const IAccessibleComPtr target) const;
  AXOptionalObject HasState(const IAccessibleComPtr target,
                            std::string state) const;

  // IAccessible2 functionality.
  AXOptionalObject GetIA2Role(IA2ComPtr target) const;
  AXOptionalObject GetIA2Attribute(const IA2ComPtr target,
                                   const AXPropertyNode& property_node) const;
  AXOptionalObject HasIA2State(const IA2ComPtr target,
                               const AXPropertyNode& property_node) const;

  // AccessibleTable functionality
  AXOptionalObject GetSelectedColumns(const IA2TableComPtr target) const;

  // IAccessibleSelectionContainer functionality.
  AXOptionalObject GetSelections(
      const IA2TextSelectionContainerComPtr target) const;
  AXOptionalObject SetSelections(const IA2TextSelectionContainerComPtr target,
                                 const AXPropertyNode& property_node) const;

  bool IsIAccessibleAndNotNull(const Target& target) const;

  // PropertyNode conversion methods.
  template <typename Interface>
  Microsoft::WRL::ComPtr<Interface> PropertyNodeToIAccessible(
      const AXPropertyNode& node) const {
    Microsoft::WRL::ComPtr<IAccessible> accessible =
        indexer_->NodeBy(node.name_or_value);
    if (!accessible) {
      return nullptr;
    }
    Microsoft::WRL::ComPtr<Interface> queried_accessible;
    HRESULT hr = accessible->QueryInterface(IID_PPV_ARGS(&queried_accessible));
    if (FAILED(hr)) {
      return nullptr;
    }
    return queried_accessible;
  }

  std::optional<IA2TextSelection> PropertyNodeToIA2TextSelection(
      const AXPropertyNode& node) const;

  std::vector<IA2TextSelection> PropertyNodeToIA2TextSelectionArray(
      const AXPropertyNode& node) const;

  // Map between IAccessible objects and their DOMIds/accessible tree
  // line numbers. Owned by the caller and outlives this object.
  const raw_ptr<const AXTreeIndexerWin> indexer_;

  // Variables storage. Owned by the caller and outlives this object.
  const raw_ptr<std::map<std::string, Target>> storage_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_CALL_STATEMENT_INVOKER_WIN_H_
