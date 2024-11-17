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
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
      const std::optional<std::vector<int>>& value);

 private:
  RoleCallbackList on_role_changed_callbacks_;

  std::unique_ptr<std::map<ax::mojom::StringAttribute,
                           std::unique_ptr<StringAttributeCallbackList>>>
      on_string_attribute_changed_callbacks_map_ = nullptr;
  std::unique_ptr<std::map<ax::mojom::IntAttribute,
                           std::unique_ptr<IntAttributeCallbackList>>>
      on_int_attribute_changed_callbacks_map_ = nullptr;
  std::unique_ptr<std::map<ax::mojom::BoolAttribute,
                           std::unique_ptr<BoolAttributeCallbackList>>>
      on_bool_attribute_changed_callbacks_map_ = nullptr;
  std::unique_ptr<
      std::map<ax::mojom::State, std::unique_ptr<StateCallbackList>>>
      on_state_changed_callbacks_map_ = nullptr;
  std::unique_ptr<std::map<ax::mojom::IntListAttribute,
                           std::unique_ptr<IntListAttributeCallbackList>>>
      on_int_list_attribute_changed_callbacks_map_ = nullptr;
};

}  // namespace ui

#endif  // UI_VIEWS_ACCESSIBILITY_AX_ATTRIBUTE_CHANGED_CALLBACKS_H_
