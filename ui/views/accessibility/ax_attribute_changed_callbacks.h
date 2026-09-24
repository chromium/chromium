// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_AX_ATTRIBUTE_CHANGED_CALLBACKS_H_
#define UI_VIEWS_ACCESSIBILITY_AX_ATTRIBUTE_CHANGED_CALLBACKS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"

namespace ui {

using RoleCallbackList = base::RepeatingCallbackList<void(ax::mojom::Role)>;
using IntAttributeCallbackList =
    base::RepeatingCallbackList<void(ax::mojom::IntAttribute,
                                     std::optional<int>)>;
using StringAttributeCallbackList =
    base::RepeatingCallbackList<void(ax::mojom::StringAttribute,
                                     const std::optional<std::string>&)>;
using BoolAttributeCallbackList =
    base::RepeatingCallbackList<void(ax::mojom::BoolAttribute,
                                     std::optional<bool>)>;
using StateCallbackList =
    base::RepeatingCallbackList<void(ax::mojom::State, bool)>;
using IntListAttributeCallbackList = base::RepeatingCallbackList<void(
    ax::mojom::IntListAttribute,
    const std::optional<std::vector<int32_t>>&)>;

class AXAttributeChangedCallbacks {
 public:
  AXAttributeChangedCallbacks();
  ~AXAttributeChangedCallbacks();

  base::CallbackListSubscription AddRoleChangedCallback(
      RoleCallbackList::CallbackType callback);
  void NotifyRoleChanged(ax::mojom::Role role);

  base::CallbackListSubscription AddStringAttributeChangedCallback(
      ax::mojom::StringAttribute attribute,
      StringAttributeCallbackList::CallbackType callback);
  void NotifyStringAttributeChanged(ax::mojom::StringAttribute attribute,
                                    const std::optional<std::string>& value);

  base::CallbackListSubscription AddIntAttributeChangedCallback(
      ax::mojom::IntAttribute attribute,
      IntAttributeCallbackList::CallbackType callback);
  void NotifyIntAttributeChanged(ax::mojom::IntAttribute attribute,
                                 std::optional<int> value);

  base::CallbackListSubscription AddBoolAttributeChangedCallback(
      ax::mojom::BoolAttribute attribute,
      BoolAttributeCallbackList::CallbackType callback);
  void NotifyBoolAttributeChanged(ax::mojom::BoolAttribute attribute,
                                  std::optional<bool> value);

  base::CallbackListSubscription AddStateChangedCallback(
      ax::mojom::State state,
      StateCallbackList::CallbackType callback);
  void NotifyStateChanged(ax::mojom::State state, bool is_enabled);

  base::CallbackListSubscription AddIntListAttributeChangedCallback(
      ax::mojom::IntListAttribute attribute,
      IntListAttributeCallbackList::CallbackType callback);
  void NotifyIntListAttributeChanged(
      ax::mojom::IntListAttribute attribute,
      const std::optional<std::vector<int32_t>>& value);

 private:
  template <typename AttributeType, typename ValueType>
  class AttributeCallbackMap {
   public:
    using CallbackList =
        base::RepeatingCallbackList<void(AttributeType, ValueType)>;

    AttributeCallbackMap();
    ~AttributeCallbackMap();

    base::CallbackListSubscription Add(
        AttributeType attribute,
        typename CallbackList::CallbackType callback);
    void Notify(AttributeType attribute, ValueType value);

   private:
    std::unique_ptr<std::map<AttributeType, CallbackList>> map_;
  };

  RoleCallbackList on_role_changed_callbacks_;

  AttributeCallbackMap<ax::mojom::StringAttribute,
                       const std::optional<std::string>&>
      string_attribute_callbacks_;
  AttributeCallbackMap<ax::mojom::IntAttribute, std::optional<int>>
      int_attribute_callbacks_;
  AttributeCallbackMap<ax::mojom::BoolAttribute, std::optional<bool>>
      bool_attribute_callbacks_;
  AttributeCallbackMap<ax::mojom::State, bool> state_callbacks_;
  AttributeCallbackMap<ax::mojom::IntListAttribute,
                       const std::optional<std::vector<int32_t>>&>
      int_list_attribute_callbacks_;
};

}  // namespace ui

#endif  // UI_VIEWS_ACCESSIBILITY_AX_ATTRIBUTE_CHANGED_CALLBACKS_H_
