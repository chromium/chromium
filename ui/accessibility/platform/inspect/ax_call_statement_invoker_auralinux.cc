// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/platform/inspect/ax_call_statement_invoker_auralinux.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_auralinux.h"
#include "ui/accessibility/platform/inspect/ax_property_node.h"

namespace ui {

std::string AXCallStatementInvokerAuraLinux::ToString(
    AXOptionalObject& optional) {
  if (optional.HasValue()) {
    Target value = *optional;

    if (absl::holds_alternative<const AtspiAccessible*>(value)) {
      return "Object";
    }
    if (absl::holds_alternative<std::string>(value)) {
      return absl::get<std::string>(value);
    }
    if (absl::holds_alternative<int>(value)) {
      return base::NumberToString(absl::get<int>(value));
    }
  }
  return optional.StateToString();
}

AXCallStatementInvokerAuraLinux::AXCallStatementInvokerAuraLinux(
    const AXTreeIndexerAuraLinux* indexer,
    std::map<std::string, Target>* storage)
    : indexer_(indexer), storage_(storage) {}

AXOptionalObject AXCallStatementInvokerAuraLinux::Invoke(
    const AXPropertyNode& property_node,
    bool no_object_parse) const {
  // TODO(alexs): failing the tests when filters are incorrect is a good idea,
  // however crashing ax_dump tools on wrong input might be not. Figure out
  // a working solution that works nicely in both cases. Use LOG(ERROR) for now
  // as a console warning.

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

      if (!IsAtspiAndNotNull(target)) {
        LOG(ERROR) << "Linux invoker only supports AtspiAccessible variable "
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
  if (!IsAtspiAndNotNull(target)) {
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

AXOptionalObject AXCallStatementInvokerAuraLinux::InvokeFor(
    const Target target,
    const AXPropertyNode& property_node) const {
  if (absl::holds_alternative<const AtspiAccessible*>(target)) {
    const AtspiAccessible* AXElement =
        absl::get<const AtspiAccessible*>(target);
    return InvokeForAXElement(AXElement, property_node);
  }

  LOG(ERROR) << "Unexpected target type for " << property_node.ToFlatString();
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerAuraLinux::GetRole(
    const AtspiAccessible* target) const {
  GError* error = nullptr;
  char* role_name = atspi_accessible_get_role_name(
      const_cast<AtspiAccessible*>(target), &error);
  if (!error) {
    return AXOptionalObject(Target(std::string(role_name)));
  }
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerAuraLinux::GetName(
    const AtspiAccessible* target) const {
  GError* error = nullptr;
  char* name =
      atspi_accessible_get_name(const_cast<AtspiAccessible*>(target), &error);
  if (!error) {
    return AXOptionalObject(Target(std::string(name)));
  }
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerAuraLinux::GetDescription(
    const AtspiAccessible* target) const {
  GError* error = nullptr;
  char* description = atspi_accessible_get_description(
      const_cast<AtspiAccessible*>(target), &error);
  if (!error) {
    return AXOptionalObject(Target(std::string(description)));
  }
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerAuraLinux::GetParent(
    const AtspiAccessible* target) const {
  GError* error = nullptr;
  AtspiAccessible* parent =
      atspi_accessible_get_parent(const_cast<AtspiAccessible*>(target), &error);
  if (!error) {
    return AXOptionalObject(Target(parent));
  }
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerAuraLinux::GetAttribute(
    const AtspiAccessible* target,
    std::string attribute) const {
  GError* error = nullptr;

  GHashTable* attributes = atspi_accessible_get_attributes(
      const_cast<AtspiAccessible*>(target), &error);
  if (!error) {
    GHashTableIter i;
    void* key = nullptr;
    void* attribute_value = nullptr;

    g_hash_table_iter_init(&i, attributes);
    while (g_hash_table_iter_next(&i, &key, &attribute_value)) {
      if (attribute.compare(static_cast<char*>(key)) == 0) {
        Target value = std::string(static_cast<char*>(attribute_value));
        return AXOptionalObject(value);
      }
    }
  }

  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerAuraLinux::HasState(
    const AtspiAccessible* target,
    std::string state) const {
  AtspiStateSet* atspi_states =
      atspi_accessible_get_state_set(const_cast<AtspiAccessible*>(target));
  GArray* state_array = atspi_state_set_get_states(atspi_states);

  for (unsigned i = 0; i < state_array->len; i++) {
    AtspiStateType state_type = g_array_index(state_array, AtspiStateType, i);
    const char* state_str = ATSPIStateToString(state_type);
    if (state.compare(state_str) == 0) {
      return AXOptionalObject(Target(true));
    }
  }
  return AXOptionalObject(Target(false));
}

AXOptionalObject AXCallStatementInvokerAuraLinux::GetRelation(
    const AtspiAccessible* target,
    std::string relation) const {
  GError* error = nullptr;

  GArray* relations = atspi_accessible_get_relation_set(
      const_cast<AtspiAccessible*>(target), &error);

  if (!error) {
    for (guint idx = 0; idx < relations->len; idx++) {
      AtspiRelation* atspi_relation =
          g_array_index(relations, AtspiRelation*, idx);
      std::string relation_str = ATSPIRelationToString(
          atspi_relation_get_relation_type(atspi_relation));
      if (relation_str.compare(relation) == 0) {
        std::string refs;
        gint total = atspi_relation_get_n_targets(atspi_relation);
        for (gint i = 0; i < total; i++) {
          auto* o = atspi_relation_get_target(atspi_relation, i);
          char* role_name = atspi_accessible_get_role_name(o, &error);
          if (!error) {
            refs.append(role_name);
            if (i < (total - 1))
              refs.append(", ");
          }
        }
        return AXOptionalObject(Target(refs));
      }
    }
  }
  return AXOptionalObject::Error();
}

AXOptionalObject AXCallStatementInvokerAuraLinux::HasInterface(
    const AtspiAccessible* target,
    std::string interface) const {
  GArray* interfaces =
      atspi_accessible_get_interfaces(const_cast<AtspiAccessible*>(target));

  for (unsigned i = 0; i < interfaces->len; i++) {
    char* iface = g_array_index(interfaces, char*, i);
    if (interface.compare(std::string(iface)) == 0) {
      return AXOptionalObject(Target(true));
    }
  }
  return AXOptionalObject(Target(false));
}

AXOptionalObject AXCallStatementInvokerAuraLinux::InvokeForAXElement(
    const AtspiAccessible* target,
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

  if (property_node.name_or_value == "parent") {
    return GetParent(target);
  }

  if (property_node.name_or_value == "getAttribute") {
    if (!property_node.arguments.size()) {
      LOG(ERROR) << "Error: " << property_node.name_or_value
                 << "called without argument";
      return AXOptionalObject::Error();
    }
    std::string attribute = property_node.arguments[0].name_or_value;
    return GetAttribute(target, attribute);
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

  if (property_node.name_or_value == "getRelation") {
    if (!property_node.arguments.size()) {
      LOG(ERROR) << "Error: " << property_node.name_or_value
                 << "called without argument";
      return AXOptionalObject::Error();
    }
    std::string relation = property_node.arguments[0].name_or_value;
    return GetRelation(target, relation);
  }

  if (property_node.name_or_value == "hasInterface") {
    if (!property_node.arguments.size()) {
      LOG(ERROR) << "Error: " << property_node.name_or_value
                 << "called without argument";
      return AXOptionalObject::Error();
    }
    std::string interface = property_node.arguments[0].name_or_value;
    return HasInterface(target, interface);
  }

  LOG(ERROR) << "Error in '" << property_node.name_or_value
             << "' called on AXElement in '" << property_node.ToFlatString()
             << "' statement";
  return AXOptionalObject::Error();
}

bool AXCallStatementInvokerAuraLinux::IsAtspiAndNotNull(Target target) const {
  auto** atspi_ptr = absl::get_if<const AtspiAccessible*>(&target);
  return atspi_ptr && *atspi_ptr;
}

}  // namespace ui
