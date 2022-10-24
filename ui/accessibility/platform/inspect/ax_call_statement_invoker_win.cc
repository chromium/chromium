// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_call_statement_invoker_win.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_win.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

namespace ui {

std::string AXCallStatementInvokerWin::ToString(AXOptionalObject& optional) {
  if (optional.HasValue()) {
    Target value = *optional;

    if (absl::holds_alternative<IAccessibleComPtr>(value))
      return "IAccessible";

    if (absl::holds_alternative<IA2ComPtr>(value))
      return "IAccessible2Interface";

    if (absl::holds_alternative<IA2HypertextComPtr>(value))
      return "IAccessible2HyperlinkInferface";

    if (absl::holds_alternative<IA2TableComPtr>(value))
      return "IAccessible2TableInterface";

    if (absl::holds_alternative<IA2TableCellComPtr>(value))
      return "IAccessible2TableCellInterface";

    if (absl::holds_alternative<IA2TextComPtr>(value))
      return "IAccessible2TextInterface";

    if (absl::holds_alternative<IA2ValueComPtr>(value))
      return "IAccessible2ValueInterface";

    if (absl::holds_alternative<std::string>(value)) {
      return "\"" + absl::get<std::string>(value) + "\"";
    }
    if (absl::holds_alternative<int>(value)) {
      return base::NumberToString(absl::get<int>(value));
    }
  }
  return optional.StateToString();
}

AXCallStatementInvokerWin::AXCallStatementInvokerWin(
    const AXTreeIndexerWin* indexer,
    std::map<std::string, Target>* storage)
    : indexer_(indexer), storage_(storage) {}

AXOptionalObject AXCallStatementInvokerWin::Invoke(
    const AXPropertyNode& property_node) const {
  // Executes a scripting statement coded in a given property node.
  // The statement represents a chainable sequence of attribute calls, where
  // each subsequent call is invoked on an object returned by a previous call.
  // For example, p.AXChildren[0].AXRole will unroll into a sequence of
  // `p.AXChildren`, `(p.AXChildren)[0]` and `((p.AXChildren)[0]).AXRole`.

  // Get an initial target to invoke an attribute for. First, check the storage
  // if it has an associated target for the property node, then query the tree
  // indexer if the property node refers to a DOM id or line index of
  // an accessible object. If the property node doesn't provide a target then
  // use the default one (if any, the default node is provided in case of
  // a tree dumping only, the scripts never have default target).
  Target target;

  // Case 1: try to get a target from the storage. The target may refer to
  // a variable which is kept in the storage.

  // For example,
  // `text_parent:= p.parent` will define `text_leaf` variable and put it
  // into the storage, and then the variable value will be extracted from
  // the storage for other instruction referring the variable, for example,
  // `text_parent.role`.
  if (storage_) {
    auto storage_iterator = storage_->find(property_node.name_or_value);
    if (storage_iterator != storage_->end()) {
      target = storage_iterator->second;

      if (!IsIAccessibleAndNotNull(target)) {
        LOG(ERROR) << "Windows invoker only supports IAccessible variable "
                      "assignments.";
        return AXOptionalObject::Error();
      }
    }
  }

  // Case 2: try to get target from the tree indexer. The target may refer to
  // an accessible element by DOM id or by a line number (:LINE_NUM format) in
  // a result accessible tree. The tree indexer keeps the mappings between
  // accessible elements and their DOM ids and line numbers.
  if (target.index() == 0) {
    target = indexer_->NodeBy(property_node.name_or_value);
  }

  // Could not find the target.
  if (!IsIAccessibleAndNotNull(target)) {
    LOG(ERROR) << "Could not find target: " << property_node.name_or_value;
    return AXOptionalObject::Error();
  }

  auto* current_node = property_node.next.get();

  // Invoke the call chain.
  while (current_node) {
    auto target_optional = InvokeFor(target, *current_node);
    // Result of the current step is state. Don't go any further.
    if (!target_optional.HasValue())
      return target_optional;

    target = *target_optional;
    current_node = current_node->next.get();
  }

  // Variable case: store the variable value in the storage.
  if (!property_node.key.empty())
    (*storage_)[property_node.key] = target;

  return AXOptionalObject(target);
}

AXOptionalObject AXCallStatementInvokerWin::InvokeFor(
    const Target target,
    const AXPropertyNode& property_node) const {
  if (absl::holds_alternative<IAccessibleComPtr>(target)) {
    IAccessibleComPtr AXElement = absl::get<IAccessibleComPtr>(target);
    return InvokeForAXElement(AXElement, property_node);
  }

  if (absl::holds_alternative<IAccessibleComPtr>(target)) {
    IAccessibleComPtr AXElement = absl::get<IAccessibleComPtr>(target);
    return InvokeForAXElement(AXElement, property_node);
  }

  if (absl::holds_alternative<IA2ComPtr>(target)) {
    IA2ComPtr ia2 = absl::get<IA2ComPtr>(target);
    return InvokeForIA2(ia2, property_node);
  }

  if (absl::holds_alternative<IA2HypertextComPtr>(target)) {
    IA2HypertextComPtr ia2hypertext = absl::get<IA2HypertextComPtr>(target);
    return InvokeForIA2Hypertext(ia2hypertext, property_node);
  }

  if (absl::holds_alternative<IA2TableComPtr>(target)) {
    IA2TableComPtr ia2table = absl::get<IA2TableComPtr>(target);
    return InvokeForIA2Table(ia2table, property_node);
  }

  if (absl::holds_alternative<IA2TableCellComPtr>(target)) {
    IA2TableCellComPtr ia2cell = absl::get<IA2TableCellComPtr>(target);
    return InvokeForIA2TableCell(ia2cell, property_node);
  }

  if (absl::holds_alternative<IA2TextComPtr>(target)) {
    IA2TextComPtr ia2text = absl::get<IA2TextComPtr>(target);
    return InvokeForIA2Text(ia2text, property_node);
  }

  if (absl::holds_alternative<IA2ValueComPtr>(target)) {
    IA2ValueComPtr ia2value = absl::get<IA2ValueComPtr>(target);
    return InvokeForIA2Value(ia2value, property_node);
  }

  LOG(ERROR) << "Unexpected target type for " << property_node.ToFlatString();
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::InvokeForAXElement(
    IAccessibleComPtr target,
    const AXPropertyNode& property_node) const {
  if (property_node.name_or_value == "role") {
    return GetRole(target);
  }

  if (property_node.name_or_value == "name") {
    return GetName(target);
  }

  if (property_node.name_or_value == "description") {
    return GetDescription(target);
  }

  if (property_node.name_or_value == "getInterface") {
    if (!property_node.arguments.size()) {
      LOG(ERROR) << "Error: " << property_node.name_or_value
                 << "called without argument";
      return AXOptionalObject::Error();
    }
    std::string interface_name = property_node.arguments[0].name_or_value;
    return GetInterface(target, interface_name);
  }

  if (property_node.name_or_value == "hasState") {
    if (!property_node.arguments.size()) {
      LOG(ERROR) << "Error: " << property_node.name_or_value
                 << "called without argument";
      return AXOptionalObject::Error();
    }
    std::string state = property_node.arguments[0].name_or_value;
    return HasState(target, state);
  }

  // Todo: add support for
  // - [ ] test.accSelection
  // - [ ] test.get_accSelection
  // - [ ] test.hasRelation(<relation>)

  LOG(ERROR) << "Error in '" << property_node.name_or_value
             << "' called on AXElement in '" << property_node.ToFlatString()
             << "' statement";
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::InvokeForIA2(
    IA2ComPtr target,
    const AXPropertyNode& property_node) const {
  if (property_node.name_or_value == "getAttribute") {
    if (!property_node.arguments.size()) {
      LOG(ERROR) << "Error: " << property_node.name_or_value
                 << "called without argument";
      return AXOptionalObject::Error();
    }
    std::string attribute = property_node.arguments[0].name_or_value;
    return GetIA2Attribute(target, attribute);
  }

  if (property_node.name_or_value == "hasState") {
    if (!property_node.arguments.size()) {
      LOG(ERROR) << "Error: " << property_node.name_or_value
                 << "called without argument";
      return AXOptionalObject::Error();
    }
    std::string state = property_node.arguments[0].name_or_value;
    return HasIA2State(target, state);
  }

  // Todo: add support for
  // - [ ] test.getInterface(IAccessible2).get_groupPosition
  // - [ ] test.getInterface(IAccessible2).get_localizedExtendedRole

  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::InvokeForIA2Hypertext(
    IA2HypertextComPtr target,
    const AXPropertyNode& property_node) const {
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::InvokeForIA2Table(
    IA2TableComPtr target,
    const AXPropertyNode& property_node) const {
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::InvokeForIA2TableCell(
    IA2TableCellComPtr target,
    const AXPropertyNode& property_node) const {
  return AXOptionalObject::Error();

  // Todo: add support for
  // - [ ] test.getInterface(IAccessibleTableCell).get_rowIndex
  // - [ ] test.getInterface(IAccessibleTableCell).get_columnIndex
  // - [ ] test.getInterface(IAccessibleTableCell).get_rowExtent
  // - [ ] test.getInterface(IAccessibleTableCell).get_columnExtent
}

AXOptionalObject AXCallStatementInvokerWin::InvokeForIA2Text(
    IA2TextComPtr target,
    const AXPropertyNode& property_node) const {
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::InvokeForIA2Value(
    IA2ValueComPtr target,
    const AXPropertyNode& property_node) const {
  return AXOptionalObject::Error();

  // Todo: add support for
  // - [ ] test.getInterface(IAccessibleValue).get_currentValue
  // - [ ] test.getInterface(IAccessibleValue).get_minimumValue
  // - [ ] test.getInterface(IAccessibleValue).get_maximumValue
}

AXOptionalObject AXCallStatementInvokerWin::GetRole(
    IAccessibleComPtr target) const {
  base::win::ScopedVariant variant_self(CHILDID_SELF);
  base::win::ScopedVariant ia_role_variant;
  if (SUCCEEDED(target->get_accRole(variant_self, ia_role_variant.Receive()))) {
    return AXOptionalObject(Target(RoleVariantToString(ia_role_variant)));
  }

  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::GetName(
    IAccessibleComPtr target) const {
  base::win::ScopedVariant variant_self(CHILDID_SELF);
  base::win::ScopedBstr name;
  auto result = target->get_accName(variant_self, name.Receive());
  if (result == S_OK)
    return AXOptionalObject(Target(base::WideToUTF8(name.Get())));
  if (result == S_FALSE)
    return AXOptionalObject(Target(std::string()));

  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::GetDescription(
    IAccessibleComPtr target) const {
  base::win::ScopedVariant variant_self(CHILDID_SELF);
  base::win::ScopedBstr desc;
  auto result = target->get_accDescription(variant_self, desc.Receive());
  if (result == S_OK)
    return AXOptionalObject(Target(base::WideToUTF8(desc.Get())));
  if (result == S_FALSE)
    return AXOptionalObject(Target(std::string()));

  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::HasState(IAccessibleComPtr target,
                                                     std::string state) const {
  base::win::ScopedVariant variant_self(CHILDID_SELF);
  int32_t ia_state = 0;
  base::win::ScopedVariant ia_state_variant;
  if (target->get_accState(variant_self, ia_state_variant.Receive()) == S_OK &&
      ia_state_variant.type() == VT_I4) {
    std::vector<std::wstring> state_strings;
    ia_state = ia_state_variant.ptr()->intVal;
    IAccessibleStateToStringVector(ia_state, &state_strings);

    for (const auto& str : state_strings) {
      if (base::WideToUTF8(str) == state)
        return AXOptionalObject(Target(true));
    }
    return AXOptionalObject(Target(false));
  }

  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::GetInterface(
    IAccessibleComPtr target,
    std::string interface_name) const {
  if (interface_name == "IAccessible2") {
    Microsoft::WRL::ComPtr<IAccessible2> ia2;
    if (S_OK == ui::IA2QueryInterface<IAccessible2>(target.Get(), &ia2))
      return AXOptionalObject(Target(ia2));
  }

  if (interface_name == "IAccessibleHypertext") {
    Microsoft::WRL::ComPtr<IAccessibleHypertext> ia2hyper;
    if (S_OK ==
        ui::IA2QueryInterface<IAccessibleHypertext>(target.Get(), &ia2hyper))
      return AXOptionalObject(Target(ia2hyper));
  }

  if (interface_name == "IAccessibleTable") {
    Microsoft::WRL::ComPtr<IAccessibleTable> ia2table;
    if (S_OK ==
        ui::IA2QueryInterface<IAccessibleTable>(target.Get(), &ia2table))
      return AXOptionalObject(Target(ia2table));
  }

  if (interface_name == "IAccessibleTableCell") {
    Microsoft::WRL::ComPtr<IAccessibleTableCell> ia2cell;
    if (S_OK ==
        ui::IA2QueryInterface<IAccessibleTableCell>(target.Get(), &ia2cell))
      return AXOptionalObject(Target(ia2cell));
  }

  if (interface_name == "IAccessibleText") {
    Microsoft::WRL::ComPtr<IAccessibleText> ia2text;
    if (S_OK == ui::IA2QueryInterface<IAccessibleText>(target.Get(), &ia2text))
      return AXOptionalObject(Target(ia2text));
  }

  if (interface_name == "IAccessibleValue") {
    Microsoft::WRL::ComPtr<IAccessibleValue> ia2value;
    if (S_OK ==
        ui::IA2QueryInterface<IAccessibleValue>(target.Get(), &ia2value))
      return AXOptionalObject(Target(ia2value));
  }

  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::GetIA2Attribute(
    IA2ComPtr target,
    std::string attribute) const {
  absl::optional<std::string> value =
      GetIAccessible2Attribute(target, attribute);
  if (value)
    return AXOptionalObject(Target(*value));
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::HasIA2State(
    IA2ComPtr target,
    std::string state) const {
  AccessibleStates states;
  if (target->get_states(&states) == S_OK) {
    std::vector<std::wstring> state_strings;
    IAccessible2StateToStringVector(states, &state_strings);

    for (const auto& str : state_strings) {
      if (base::WideToUTF8(str) == state)
        return AXOptionalObject(Target(true));
    }
    return AXOptionalObject(Target(false));
  }

  return AXOptionalObject::Error();
}

bool AXCallStatementInvokerWin::IsIAccessibleAndNotNull(Target target) const {
  if (IAccessibleComPtr* ia_ptr = absl::get_if<IAccessibleComPtr>(&target)) {
    if ((*ia_ptr).Get() != nullptr)
      return true;
  }
  return false;
}

}  // namespace ui
