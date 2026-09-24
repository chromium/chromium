// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_attribute_changed_callbacks.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

template <typename AttributeType, typename ValueType>
AXAttributeChangedCallbacks::AttributeCallbackMap<AttributeType, ValueType>::
    AttributeCallbackMap() = default;

template <typename AttributeType, typename ValueType>
AXAttributeChangedCallbacks::AttributeCallbackMap<AttributeType, ValueType>::
    ~AttributeCallbackMap() = default;

template <typename AttributeType, typename ValueType>
base::CallbackListSubscription AXAttributeChangedCallbacks::
    AttributeCallbackMap<AttributeType, ValueType>::Add(
        AttributeType attribute,
        typename CallbackList::CallbackType callback) {
  if (!map_) {
    map_ = std::make_unique<std::map<AttributeType, CallbackList>>();
  }
  return (*map_)[attribute].Add(std::move(callback));
}

template <typename AttributeType, typename ValueType>
void AXAttributeChangedCallbacks::AttributeCallbackMap<
    AttributeType,
    ValueType>::Notify(AttributeType attribute, ValueType value) {
  if (map_) {
    auto it = map_->find(attribute);
    if (it != map_->end()) {
      it->second.Notify(attribute, value);
    }
  }
}

AXAttributeChangedCallbacks::AXAttributeChangedCallbacks() = default;

AXAttributeChangedCallbacks::~AXAttributeChangedCallbacks() = default;

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddRoleChangedCallback(
    RoleCallbackList::CallbackType callback) {
  return on_role_changed_callbacks_.Add(std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyRoleChanged(ax::mojom::Role role) {
  on_role_changed_callbacks_.Notify(role);
}

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddStringAttributeChangedCallback(
    ax::mojom::StringAttribute attribute,
    StringAttributeCallbackList::CallbackType callback) {
  return string_attribute_callbacks_.Add(attribute, std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyStringAttributeChanged(
    ax::mojom::StringAttribute attribute,
    const std::optional<std::string>& value) {
  string_attribute_callbacks_.Notify(attribute, value);
}

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddIntAttributeChangedCallback(
    ax::mojom::IntAttribute attribute,
    IntAttributeCallbackList::CallbackType callback) {
  return int_attribute_callbacks_.Add(attribute, std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyIntAttributeChanged(
    ax::mojom::IntAttribute attribute,
    std::optional<int> value) {
  int_attribute_callbacks_.Notify(attribute, value);
}

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddBoolAttributeChangedCallback(
    ax::mojom::BoolAttribute attribute,
    BoolAttributeCallbackList::CallbackType callback) {
  return bool_attribute_callbacks_.Add(attribute, std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyBoolAttributeChanged(
    ax::mojom::BoolAttribute attribute,
    std::optional<bool> value) {
  bool_attribute_callbacks_.Notify(attribute, value);
}

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddStateChangedCallback(
    ax::mojom::State state,
    StateCallbackList::CallbackType callback) {
  return state_callbacks_.Add(state, std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyStateChanged(ax::mojom::State state,
                                                     bool value) {
  state_callbacks_.Notify(state, value);
}

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddIntListAttributeChangedCallback(
    ax::mojom::IntListAttribute attribute,
    IntListAttributeCallbackList::CallbackType callback) {
  return int_list_attribute_callbacks_.Add(attribute, std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyIntListAttributeChanged(
    ax::mojom::IntListAttribute attribute,
    const std::optional<std::vector<int32_t>>& value) {
  int_list_attribute_callbacks_.Notify(attribute, value);
}

}  // namespace ui
