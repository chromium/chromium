// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/inspect/ax_call_statement_invoker_win.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_win.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

#define DEFINE_IA2_QI_ENTRY(ia2_interface)                                    \
  if (interface_name == #ia2_interface) {                                     \
    Microsoft::WRL::ComPtr<ia2_interface> obj;                                \
    HRESULT hr = IA2QueryInterface<ia2_interface>(target.Get(), &obj);        \
    if (hr == S_OK)                                                           \
      return AXOptionalObject({obj});                                         \
    if (hr == E_NOINTERFACE)                                                  \
      return AXOptionalObject::Error(interface_name + " is not implemented"); \
    return AXOptionalObject::Error("Unexpected error when querying " +        \
                                   interface_name);                           \
  }

#define CHECK_ARGS_N(arg_count, property_node)                         \
  if (property_node.arguments.size() < arg_count) {                    \
    return AXOptionalObject::Error("too few arguments to function " +  \
                                   property_node.name_or_value);       \
  }                                                                    \
  if (property_node.arguments.size() > arg_count) {                    \
    return AXOptionalObject::Error("too many arguments to function " + \
                                   property_node.name_or_value);       \
  }

#define CHECK_ARGS_1(property_node) CHECK_ARGS_N(1, property_node)

namespace ui {

// static
std::string AXCallStatementInvokerWin::ToString(
    const AXOptionalObject& optional) {
  if (optional.HasValue()) {
    return optional->ToString();
  }

  if (optional.IsError()) {
    return "Error:\"" + optional.StateText() + "\"";
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
  if (!target) {
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
    const Target& target,
    const AXPropertyNode& property_node) const {
  if (target.Is<IAccessibleComPtr>())
    return InvokeForAXElement(target.As<IAccessibleComPtr>(), property_node);

  if (target.Is<IA2ComPtr>())
    return InvokeForIA2(target.As<IA2ComPtr>(), property_node);

  if (target.Is<IA2HypertextComPtr>())
    return InvokeForIA2Hypertext(target.As<IA2HypertextComPtr>(),
                                 property_node);

  if (target.Is<IA2TableComPtr>())
    return InvokeForIA2Table(target.As<IA2TableComPtr>(), property_node);

  if (target.Is<IA2TableCellComPtr>())
    return InvokeForIA2TableCell(target.As<IA2TableCellComPtr>(),
                                 property_node);

  if (target.Is<IA2TextComPtr>())
    return InvokeForIA2Text(target.As<IA2TextComPtr>(), property_node);

  if (target.Is<IA2TextSelectionContainerComPtr>()) {
    return InvokeForIA2TextSelectionContainer(
        target.As<IA2TextSelectionContainerComPtr>(), property_node);
  }

  if (target.Is<IA2ValueComPtr>())
    return InvokeForIA2Value(target.As<IA2ValueComPtr>(), property_node);

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

  if (property_node.name_or_value == "QueryInterface") {
    if (!property_node.arguments.size()) {
      LOG(ERROR) << "Error: " << property_node.name_or_value
                 << "called without argument";
      return AXOptionalObject::Error();
    }
    std::string interface_name = property_node.arguments[0].name_or_value;
    return QueryInterface(target, interface_name);
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
  if (property_node.name_or_value == "role")
    return GetIA2Role(target);

  if (property_node.name_or_value == "getAttribute") {
    return GetIA2Attribute(target, property_node);
  }

  if (property_node.name_or_value == "hasState") {
    return HasIA2State(target, property_node);
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
  if (property_node.name_or_value == "selectedColumns") {
    return GetSelectedColumns(target);
  }

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

AXOptionalObject AXCallStatementInvokerWin::InvokeForIA2TextSelectionContainer(
    IA2TextSelectionContainerComPtr target,
    const AXPropertyNode& property_node) const {
  if (property_node.name_or_value == "selections") {
    return GetSelections(target);
  }

  if (property_node.name_or_value == "setSelections") {
    return SetSelections(target, property_node);
  }

  return AXOptionalObject::Error();
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

AXOptionalObject AXCallStatementInvokerWin::QueryInterface(
    IAccessibleComPtr target,
    std::string interface_name) const {
  DEFINE_IA2_QI_ENTRY(IAccessible2)
  DEFINE_IA2_QI_ENTRY(IAccessibleHypertext)
  DEFINE_IA2_QI_ENTRY(IAccessibleTable)
  DEFINE_IA2_QI_ENTRY(IAccessibleTableCell)
  DEFINE_IA2_QI_ENTRY(IAccessibleTextSelectionContainer)
  DEFINE_IA2_QI_ENTRY(IAccessibleText)
  DEFINE_IA2_QI_ENTRY(IAccessibleValue)

  return AXOptionalObject::Error("Unsupported " + interface_name +
                                 " interface");
}

AXOptionalObject AXCallStatementInvokerWin::GetIA2Role(IA2ComPtr target) const {
  LONG role = 0;
  if (SUCCEEDED(target->role(&role)))
    return AXOptionalObject(
        Target(base::WideToUTF8(IAccessible2RoleToString(role))));

  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::GetIA2Attribute(
    IA2ComPtr target,
    const AXPropertyNode& property_node) const {
  CHECK_ARGS_1(property_node)

  std::string attribute = property_node.arguments[0].name_or_value;
  std::optional<std::string> value =
      GetIAccessible2Attribute(target, attribute);
  if (value)
    return AXOptionalObject(Target(*value));
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::HasIA2State(
    IA2ComPtr target,
    const AXPropertyNode& property_node) const {
  CHECK_ARGS_1(property_node)
  std::string state = property_node.arguments[0].name_or_value;

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

AXOptionalObject AXCallStatementInvokerWin::GetSelectedColumns(
    const IA2TableComPtr target) const {
  ScopedCoMemArray<LONG> columns;
  if (target->get_selectedColumns(INT_MAX, columns.Receive(),
                                  columns.ReceiveSize()) == S_OK) {
    return AXOptionalObject({std::move(columns)});
  }
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::GetSelections(
    IA2TextSelectionContainerComPtr target) const {
  ScopedCoMemArray<IA2TextSelection> selections;
  if (target->get_selections(selections.Receive(), selections.ReceiveSize()) ==
      S_OK) {
    return AXOptionalObject({std::move(selections)});
  }
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerWin::SetSelections(
    const IA2TextSelectionContainerComPtr target,
    const AXPropertyNode& property_node) const {
  CHECK_ARGS_1(property_node)

  std::vector<IA2TextSelection> selections =
      PropertyNodeToIA2TextSelectionArray(property_node.arguments[0]);
  if (selections.size() == 0) {
    return AXOptionalObject::Error("Empty IA2TextSelection array is given");
  }

  if (target->setSelections(selections.size(), selections.data()) == S_OK) {
    return AXOptionalObject({target});
  }

  return AXOptionalObject::Error();
}

bool AXCallStatementInvokerWin::IsIAccessibleAndNotNull(
    const Target& target) const {
  return target.Is<IAccessibleComPtr>() &&
         target.As<IAccessibleComPtr>().Get() != nullptr;
}

std::optional<IA2TextSelection>
AXCallStatementInvokerWin::PropertyNodeToIA2TextSelection(
    const AXPropertyNode& node) const {
  if (!node.IsDict()) {
    return std::nullopt;
  }

  const AXPropertyNode* start_obj_node = node.FindKey("startObj");
  if (!start_obj_node) {
    return std::nullopt;
  }

  IA2TextComPtr start_obj =
      PropertyNodeToIAccessible<IAccessibleText>(*start_obj_node);
  if (!start_obj) {
    return std::nullopt;
  }

  std::optional<int> start_offset = node.FindIntKey("startOffset");
  if (!start_offset) {
    return std::nullopt;
  }

  const AXPropertyNode* end_obj_node = node.FindKey("endObj");
  if (!end_obj_node) {
    return std::nullopt;
  }

  IA2TextComPtr end_obj =
      PropertyNodeToIAccessible<IAccessibleText>(*end_obj_node);
  if (!end_obj) {
    return std::nullopt;
  }

  std::optional<int> end_offset = node.FindIntKey("endOffset");
  if (!end_offset) {
    return std::nullopt;
  }

  IA2TextSelection text_selection{
      start_obj.Detach(),
      *start_offset,
      end_obj.Detach(),
      *end_offset,
  };
  return {std::move(text_selection)};
}

std::vector<IA2TextSelection>
AXCallStatementInvokerWin::PropertyNodeToIA2TextSelectionArray(
    const AXPropertyNode& node) const {
  if (!node.IsArray()) {
    return {};
  }

  std::vector<IA2TextSelection> array;
  for (const auto& item_node : node.arguments) {
    std::optional<IA2TextSelection> item =
        PropertyNodeToIA2TextSelection(item_node);
    if (!item) {
      return {};
    }
    array.push_back(std::move(*item));
  }
  return array;
}

}  // namespace ui
