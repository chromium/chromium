// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/action_view_controller.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace {
const std::u16string kActionTextDisabled = u"Test Action Disabled";
const std::u16string kActionTextEnabled = u"Test Action Enabled";
constexpr int kTestActionIdDisabled = 0;
constexpr int kTestActionIdEnabled = 1;

std::unique_ptr<actions::ActionItem> CreateDisabledActionItem() {
  return actions::ActionItem::Builder()
      .SetText(kActionTextDisabled)
      .SetActionId(kTestActionIdDisabled)
      .SetVisible(true)
      .SetEnabled(false)
      .Build();
}

std::unique_ptr<actions::ActionItem> CreateEnabledActionItem() {
  return actions::ActionItem::Builder()
      .SetText(kActionTextEnabled)
      .SetActionId(kTestActionIdEnabled)
      .SetVisible(true)
      .SetEnabled(true)
      .Build();
}

}  // namespace

namespace views {

using ActionViewControllerTest = ViewsTestBase;

// Test changing the action item will trigger the action changed callback on
// both the derived and base class controller.
TEST_F(ActionViewControllerTest, TestActionChangedCallbackCalled) {
  std::unique_ptr<actions::ActionItem> action_item = CreateDisabledActionItem();
  auto action_view = std::make_unique<MdTextButton>();
  EXPECT_EQ(action_view->GetText(), u"");
  EXPECT_TRUE(action_view->GetEnabled());
  auto action_view_controller =
      std::make_unique<ActionViewController<MdTextButton>>(
          action_view.get(), action_item->GetAsWeakPtr());
  EXPECT_EQ(action_view->GetText(), kActionTextDisabled);
  EXPECT_FALSE(action_view->GetEnabled());
  action_item->SetText(kActionTextEnabled);
  action_item->SetEnabled(true);
  EXPECT_EQ(action_view->GetText(), kActionTextEnabled);
  EXPECT_TRUE(action_view->GetEnabled());
}

// Test reassigning to variable of base class type still has access to action
// item and action views
TEST_F(ActionViewControllerTest, TestReasignToBaseClass) {
  std::unique_ptr<actions::ActionItem> action_item = CreateDisabledActionItem();
  auto action_view = std::make_unique<MdTextButton>();
  auto action_view_controller =
      std::make_unique<ActionViewController<MdTextButton>>(
          action_view.get(), action_item->GetAsWeakPtr());
  std::unique_ptr<ActionViewController<View>> base_action_view_controller =
      std::move(action_view_controller);
  EXPECT_NE(base_action_view_controller->GetActionView(), nullptr);
  EXPECT_NE(base_action_view_controller->GetActionItemForTesting(), nullptr);
}

// Test reassigning action item.
TEST_F(ActionViewControllerTest, TestReassignActionItem) {
  std::unique_ptr<actions::ActionItem> disabled_action_item =
      CreateDisabledActionItem();
  auto action_view = std::make_unique<MdTextButton>();
  auto action_view_controller =
      std::make_unique<ActionViewController<MdTextButton>>(
          action_view.get(), disabled_action_item->GetAsWeakPtr());
  EXPECT_EQ(action_view->GetText(), kActionTextDisabled);
  EXPECT_FALSE(action_view->GetEnabled());
  std::unique_ptr<actions::ActionItem> enabled_action_item =
      CreateEnabledActionItem();
  action_view_controller->SetActionItem(enabled_action_item->GetAsWeakPtr());
  EXPECT_EQ(action_view->GetText(), kActionTextEnabled);
  EXPECT_TRUE(action_view->GetEnabled());
}

// Test reassigning action view.
TEST_F(ActionViewControllerTest, TestReassignActionView) {
  std::unique_ptr<actions::ActionItem> action_item = CreateDisabledActionItem();
  auto first_action_view = std::make_unique<MdTextButton>();
  auto action_view_controller =
      std::make_unique<ActionViewController<MdTextButton>>(
          first_action_view.get(), action_item->GetAsWeakPtr());
  EXPECT_EQ(first_action_view->GetText(), kActionTextDisabled);
  EXPECT_FALSE(first_action_view->GetEnabled());
  auto second_action_view = std::make_unique<MdTextButton>();
  action_view_controller->SetActionView(second_action_view.get());
  action_item->SetEnabled(true);
  EXPECT_FALSE(first_action_view->GetEnabled());
  EXPECT_TRUE(second_action_view->GetEnabled());
}

// Test that a destroyed view does not cause crashes when action item change
// triggered.
TEST_F(ActionViewControllerTest, TestActionViewDestroyed) {
  std::unique_ptr<actions::ActionItem> action_item = CreateDisabledActionItem();
  auto action_view = std::make_unique<MdTextButton>();
  auto action_view_controller =
      std::make_unique<ActionViewController<MdTextButton>>(
          action_view.get(), action_item->GetAsWeakPtr());
  action_view.reset();
  action_item->SetEnabled(true);
}

// Test that action triggered callbacks get called.
TEST_F(ActionViewControllerTest, TriggerAction) {
  std::unique_ptr<actions::ActionItem> action_item = CreateEnabledActionItem();
  auto action_view = std::make_unique<MdTextButton>();
  auto action_view_controller =
      std::make_unique<ActionViewController<MdTextButton>>(
          action_view.get(), action_item->GetAsWeakPtr());
  action_view_controller->SetActionItem(action_item->GetAsWeakPtr());
  EXPECT_EQ(0, action_item->GetInvokeCount());
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(action_view.get());
  test_api.NotifyClick(e);
  EXPECT_EQ(1, action_item->GetInvokeCount());
}

}  // namespace views
