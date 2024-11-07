// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/ax_attribute_changed_callbacks.h"

#include <utility>

#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ui {

using StringAttrCallbackPair =
    std::pair<ax::mojom::StringAttribute,
              std::unique_ptr<StringAttributeCallbackList>>;
using IntAttrCallbackPair =
    std::pair<ax::mojom::IntAttribute,
              std::unique_ptr<IntAttributeCallbackList>>;
using BoolAttrCallbackPair =
    std::pair<ax::mojom::BoolAttribute,
              std::unique_ptr<BoolAttributeCallbackList>>;
using StateCallbackPair =
    std::pair<ax::mojom::State, std::unique_ptr<StateCallbackList>>;
using IntListAttrCallbackPair =
    std::pair<ax::mojom::IntListAttribute,
              std::unique_ptr<IntListAttributeCallbackList>>;

AXAttributeChangedCallbacks::AXAttributeChangedCallbacks() = default;

AXAttributeChangedCallbacks::~AXAttributeChangedCallbacks() = default;

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddRoleChangedCallback(
    RoleCallbackList::CallbackType callback) {
  return on_role_changed_callbacks_.Add(callback);
}

void AXAttributeChangedCallbacks::NotifyRoleChanged(ax::mojom::Role role) {
  on_role_changed_callbacks_.Notify(role);
}

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddStringAttributeChangedCallback(
    ax::mojom::StringAttribute attribute,
    StringAttributeCallbackList::CallbackType callback) {
  if (!on_string_attribute_changed_callbacks_map_) {
    on_string_attribute_changed_callbacks_map_ = std::make_unique<
        std::map<ax::mojom::StringAttribute,
                 std::unique_ptr<StringAttributeCallbackList>>>();
  }

  auto it = on_string_attribute_changed_callbacks_map_->find(attribute);
  if (it == on_string_attribute_changed_callbacks_map_->end()) {
    return on_string_attribute_changed_callbacks_map_
        ->insert(StringAttrCallbackPair(
            attribute, std::make_unique<StringAttributeCallbackList>()))
        .first->second->Add(std::move(callback));
  }

  return it->second->Add(std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyStringAttributeChanged(
    ax::mojom::StringAttribute attribute,
    const std::optional<std::string>& value) {
  if (on_string_attribute_changed_callbacks_map_) {
    auto it = on_string_attribute_changed_callbacks_map_->find(attribute);

    if (it != on_string_attribute_changed_callbacks_map_->end()) {
      it->second->Notify(attribute, value);
    }
  }
}

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddIntAttributeChangedCallback(
    ax::mojom::IntAttribute attribute,
    IntAttributeCallbackList::CallbackType callback) {
  if (!on_int_attribute_changed_callbacks_map_) {
    on_int_attribute_changed_callbacks_map_ =
        std::make_unique<std::map<ax::mojom::IntAttribute,
                                  std::unique_ptr<IntAttributeCallbackList>>>();
  }

  auto it = on_int_attribute_changed_callbacks_map_->find(attribute);
  if (it == on_int_attribute_changed_callbacks_map_->end()) {
    return on_int_attribute_changed_callbacks_map_
        ->insert(IntAttrCallbackPair(
            attribute, std::make_unique<IntAttributeCallbackList>()))
        .first->second->Add(std::move(callback));
  }

  return it->second->Add(std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyIntAttributeChanged(
    ax::mojom::IntAttribute attribute,
    std::optional<int> value) {
  if (on_int_attribute_changed_callbacks_map_) {
    auto it = on_int_attribute_changed_callbacks_map_->find(attribute);

    if (it != on_int_attribute_changed_callbacks_map_->end()) {
      it->second->Notify(attribute, value);
    }
  }
}

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddBoolAttributeChangedCallback(
    ax::mojom::BoolAttribute attribute,
    BoolAttributeCallbackList::CallbackType callback) {
  if (!on_bool_attribute_changed_callbacks_map_) {
    on_bool_attribute_changed_callbacks_map_ = std::make_unique<
        std::map<ax::mojom::BoolAttribute,
                 std::unique_ptr<BoolAttributeCallbackList>>>();
  }

  auto it = on_bool_attribute_changed_callbacks_map_->find(attribute);
  if (it == on_bool_attribute_changed_callbacks_map_->end()) {
    return on_bool_attribute_changed_callbacks_map_
        ->insert(BoolAttrCallbackPair(
            attribute, std::make_unique<BoolAttributeCallbackList>()))
        .first->second->Add(std::move(callback));
  }

  return it->second->Add(std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyBoolAttributeChanged(
    ax::mojom::BoolAttribute attribute,
    std::optional<bool> value) {
  if (on_bool_attribute_changed_callbacks_map_) {
    auto it = on_bool_attribute_changed_callbacks_map_->find(attribute);

    if (it != on_bool_attribute_changed_callbacks_map_->end()) {
      it->second->Notify(attribute, value);
    }
  }
}

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddStateChangedCallback(
    ax::mojom::State state,
    StateCallbackList::CallbackType callback) {
  if (!on_state_changed_callbacks_map_) {
    on_state_changed_callbacks_map_ = std::make_unique<
        std::map<ax::mojom::State, std::unique_ptr<StateCallbackList>>>();
  }

  auto it = on_state_changed_callbacks_map_->find(state);
  if (it == on_state_changed_callbacks_map_->end()) {
    return on_state_changed_callbacks_map_
        ->insert(
            StateCallbackPair(state, std::make_unique<StateCallbackList>()))
        .first->second->Add(std::move(callback));
  }

  return it->second->Add(std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyStateChanged(ax::mojom::State state,
                                                     bool value) {
  if (on_state_changed_callbacks_map_) {
    auto it = on_state_changed_callbacks_map_->find(state);

    if (it != on_state_changed_callbacks_map_->end()) {
      it->second->Notify(state, value);
    }
  }
}

base::CallbackListSubscription
AXAttributeChangedCallbacks::AddIntListAttributeChangedCallback(
    ax::mojom::IntListAttribute attribute,
    IntListAttributeCallbackList::CallbackType callback) {
  if (!on_int_list_attribute_changed_callbacks_map_) {
    on_int_list_attribute_changed_callbacks_map_ = std::make_unique<
        std::map<ax::mojom::IntListAttribute,
                 std::unique_ptr<IntListAttributeCallbackList>>>();
  }

  auto it = on_int_list_attribute_changed_callbacks_map_->find(attribute);
  if (it == on_int_list_attribute_changed_callbacks_map_->end()) {
    return on_int_list_attribute_changed_callbacks_map_
        ->insert(IntListAttrCallbackPair(
            attribute, std::make_unique<IntListAttributeCallbackList>()))
        .first->second->Add(std::move(callback));
  }

  return it->second->Add(std::move(callback));
}

void AXAttributeChangedCallbacks::NotifyIntListAttributeChanged(
    ax::mojom::IntListAttribute attribute,
    const std::optional<std::vector<int>>& value) {
  if (on_int_list_attribute_changed_callbacks_map_) {
    auto it = on_int_list_attribute_changed_callbacks_map_->find(attribute);

    if (it != on_int_list_attribute_changed_callbacks_map_->end()) {
      it->second->Notify(attribute, value);
    }
  }
}

}  // namespace ui
