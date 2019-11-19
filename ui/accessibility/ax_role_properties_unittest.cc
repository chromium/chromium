// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unordered_set>

#include "testing/gtest/include/gtest/gtest.h"

#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_role_properties.h"

namespace ui {

TEST(AXRolePropertiesTest, TestIsClickable) {
  // Test for ax node data attribute with a custom default action verb.
  AXNodeData data_default_action_verb;

  for (int action_verb_idx =
           static_cast<int>(ax::mojom::DefaultActionVerb::kMinValue);
       action_verb_idx <=
       static_cast<int>(ax::mojom::DefaultActionVerb::kMaxValue);
       action_verb_idx++) {
    data_default_action_verb.SetDefaultActionVerb(
        static_cast<ax::mojom::DefaultActionVerb>(action_verb_idx));
    bool is_clickable = IsClickable(data_default_action_verb);

    SCOPED_TRACE(testing::Message()
                 << "ax::mojom::DefaultActionVerb="
                 << ToString(data_default_action_verb.GetDefaultActionVerb())
                 << ", Actual: isClickable=" << is_clickable
                 << ", Expected: isClickable=" << !is_clickable);

    if (data_default_action_verb.GetDefaultActionVerb() ==
            ax::mojom::DefaultActionVerb::kClickAncestor ||
        data_default_action_verb.GetDefaultActionVerb() ==
            ax::mojom::DefaultActionVerb::kNone)
      EXPECT_FALSE(is_clickable);
    else
      EXPECT_TRUE(is_clickable);
  }

  // Test for iterating through all roles and validate if a role is clickable.
  std::unordered_set<ax::mojom::Role> roles_expected_is_clickable = {
      ax::mojom::Role::kButton,
      ax::mojom::Role::kCheckBox,
      ax::mojom::Role::kColorWell,
      ax::mojom::Role::kDisclosureTriangle,
      ax::mojom::Role::kDocBackLink,
      ax::mojom::Role::kDocBiblioRef,
      ax::mojom::Role::kDocGlossRef,
      ax::mojom::Role::kDocNoteRef,
      ax::mojom::Role::kLink,
      ax::mojom::Role::kListBoxOption,
      ax::mojom::Role::kMenuButton,
      ax::mojom::Role::kMenuItem,
      ax::mojom::Role::kMenuItemCheckBox,
      ax::mojom::Role::kMenuItemRadio,
      ax::mojom::Role::kMenuListOption,
      ax::mojom::Role::kMenuListPopup,
      ax::mojom::Role::kPopUpButton,
      ax::mojom::Role::kRadioButton,
      ax::mojom::Role::kSwitch,
      ax::mojom::Role::kTab,
      ax::mojom::Role::kToggleButton};

  AXNodeData data;

  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    data.role = static_cast<ax::mojom::Role>(role_idx);
    bool is_clickable = IsClickable(data);

    SCOPED_TRACE(testing::Message()
                 << "ax::mojom::Role=" << ToString(data.role)
                 << ", Actual: isClickable=" << is_clickable
                 << ", Expected: isClickable=" << !is_clickable);

    if (roles_expected_is_clickable.find(data.role) !=
        roles_expected_is_clickable.end())
      EXPECT_TRUE(is_clickable);
    else
      EXPECT_FALSE(is_clickable);
  }
}

TEST(AXRolePropertiesTest, TestIsInvokable) {
  // Test for iterating through all roles and validate if a role is invokable.
  // A role is invokable if it is clickable and supports neither expand collpase
  // nor toggle.
  AXNodeData data;
  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    data.role = static_cast<ax::mojom::Role>(role_idx);
    bool supports_expand_collapse = SupportsExpandCollapse(data);
    bool supports_toggle = SupportsToggle(data.role);
    bool is_clickable = IsClickable(data);
    bool is_invokable = IsInvokable(data);

    SCOPED_TRACE(testing::Message()
                 << "ax::mojom::Role=" << ToString(data.role)
                 << ", isClickable=" << is_clickable
                 << ", supportsToggle=" << supports_toggle
                 << ", supportsExpandCollapse=" << supports_expand_collapse
                 << ", Actual: isInvokable=" << is_invokable
                 << ", Expected: isInvokable=" << !is_invokable);

    if (is_clickable && !supports_toggle && !supports_expand_collapse)
      EXPECT_TRUE(is_invokable);
    else
      EXPECT_FALSE(is_invokable);
  }
}

TEST(AXRolePropertiesTest, TestSupportsExpandCollapse) {
  // Test for iterating through all hasPopup attributes and validate if a
  // hasPopup attribute supports expand collapse.
  AXNodeData data_has_popup;

  for (int has_popup_idx = static_cast<int>(ax::mojom::HasPopup::kMinValue);
       has_popup_idx <= static_cast<int>(ax::mojom::HasPopup::kMaxValue);
       has_popup_idx++) {
    data_has_popup.SetHasPopup(static_cast<ax::mojom::HasPopup>(has_popup_idx));
    bool supports_expand_collapse = SupportsExpandCollapse(data_has_popup);

    SCOPED_TRACE(testing::Message() << "ax::mojom::HasPopup="
                                    << ToString(data_has_popup.GetHasPopup())
                                    << ", Actual: supportsExpandCollapse="
                                    << supports_expand_collapse
                                    << ", Expected: supportsExpandCollapse="
                                    << !supports_expand_collapse);

    if (data_has_popup.GetHasPopup() == ax::mojom::HasPopup::kFalse)
      EXPECT_FALSE(supports_expand_collapse);
    else
      EXPECT_TRUE(supports_expand_collapse);
  }

  // Test for iterating through all states and validate if a state supports
  // expand collapse.
  AXNodeData data_state;

  for (int state_idx = static_cast<int>(ax::mojom::State::kMinValue);
       state_idx <= static_cast<int>(ax::mojom::State::kMaxValue);
       state_idx++) {
    ax::mojom::State state = static_cast<ax::mojom::State>(state_idx);

    // skipping kNone here because AXNodeData::AddState, RemoveState forbids
    // kNone to be added/removed and would fail DCHECK.
    if (state == ax::mojom::State::kNone)
      continue;

    data_state.AddState(state);

    bool supports_expand_collapse = SupportsExpandCollapse(data_state);

    SCOPED_TRACE(testing::Message() << "ax::mojom::State=" << ToString(state)
                                    << ", Actual: supportsExpandCollapse="
                                    << supports_expand_collapse
                                    << ", Expected: supportsExpandCollapse="
                                    << !supports_expand_collapse);

    if (data_state.HasState(ax::mojom::State::kExpanded) ||
        data_state.HasState(ax::mojom::State::kCollapsed))
      EXPECT_TRUE(supports_expand_collapse);
    else
      EXPECT_FALSE(supports_expand_collapse);

    data_state.RemoveState(state);
  }

  // Test for iterating through all roles and validate if a role supports expand
  // collapse.
  AXNodeData data;

  std::unordered_set<ax::mojom::Role> roles_expected_supports_expand_collapse =
      {ax::mojom::Role::kComboBoxGrouping, ax::mojom::Role::kComboBoxMenuButton,
       ax::mojom::Role::kDisclosureTriangle,
       ax::mojom::Role::kTextFieldWithComboBox, ax::mojom::Role::kTreeItem};

  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    data.role = static_cast<ax::mojom::Role>(role_idx);
    bool supports_expand_collapse = SupportsExpandCollapse(data);

    SCOPED_TRACE(testing::Message() << "ax::mojom::Role=" << ToString(data.role)
                                    << ", Actual: supportsExpandCollapse="
                                    << supports_expand_collapse
                                    << ", Expected: supportsExpandCollapse="
                                    << !supports_expand_collapse);

    if (roles_expected_supports_expand_collapse.find(data.role) !=
        roles_expected_supports_expand_collapse.end())
      EXPECT_TRUE(supports_expand_collapse);
    else
      EXPECT_FALSE(supports_expand_collapse);
  }
}

TEST(AXRolePropertiesTest, TestSupportsToggle) {
  // Test for iterating through all roles and validate if a role supports
  // toggle.
  std::unordered_set<ax::mojom::Role> roles_expected_supports_toggle = {
      ax::mojom::Role::kCheckBox, ax::mojom::Role::kMenuItemCheckBox,
      ax::mojom::Role::kSwitch, ax::mojom::Role::kToggleButton};

  for (int role_idx = static_cast<int>(ax::mojom::Role::kMinValue);
       role_idx <= static_cast<int>(ax::mojom::Role::kMaxValue); role_idx++) {
    ax::mojom::Role role = static_cast<ax::mojom::Role>(role_idx);
    bool supports_toggle = SupportsToggle(role);

    SCOPED_TRACE(testing::Message()
                 << "ax::mojom::Role=" << ToString(role)
                 << ", Actual: supportsToggle=" << supports_toggle
                 << ", Expected: supportsToggle=" << !supports_toggle);

    if (roles_expected_supports_toggle.find(role) !=
        roles_expected_supports_toggle.end())
      EXPECT_TRUE(supports_toggle);
    else
      EXPECT_FALSE(supports_toggle);
  }
}

}  // namespace ui
