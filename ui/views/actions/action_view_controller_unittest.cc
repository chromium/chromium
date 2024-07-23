// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/actions/action_view_controller.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/class_property.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/actions/action_view_interface.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kTestPropertyValueKey, false)

// More specific functionalities are tested for each individual
// ActionViewInterface.
class TestButtonActionViewInterface : public views::ButtonActionViewInterface {
 public:
  explicit TestButtonActionViewInterface(views::Button* action_view)
      : ButtonActionViewInterface(action_view), action_view_(action_view) {}

  // ButtonActionViewInterface:
  void InvokeActionImpl(actions::ActionItem* action_item) override {
    action_item->InvokeAction(actions::ActionInvocationContext::Builder()
                                  .SetProperty(kTestPropertyValueKey, true)
                                  .Build());
  }

  void OnViewChangedImpl(actions::ActionItem* action_item) override {
    // Dummy logic that is computed using both view and action item.
    if (action_item->GetVisible() && action_view_->GetVisible()) {
      action_item->SetVisible(false);
    }
  }

 private:
  raw_ptr<views::Button> action_view_;
};

class TestActionButton : public views::Button {
  METADATA_HEADER(TestActionButton, Button)
 public:
  TestActionButton() = default;
  ~TestActionButton() override = default;

  // View:
  std::unique_ptr<views::ActionViewInterface> GetActionViewInterface()
      override {
    return std::make_unique<TestButtonActionViewInterface>(this);
  }
};

BEGIN_METADATA(TestActionButton)
END_METADATA

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

// Test reassigning action item.
TEST_F(ActionViewControllerTest, TestReassignActionItem) {
  std::unique_ptr<actions::ActionItem> disabled_action_item =
      CreateDisabledActionItem();
  auto action_view = std::make_unique<MdTextButton>();
  auto action_view_controller =
      std::make_unique<ActionViewControllerTemplate<MdTextButton>>(
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
      std::make_unique<ActionViewControllerTemplate<MdTextButton>>(
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
      std::make_unique<ActionViewControllerTemplate<MdTextButton>>(
          action_view.get(), action_item->GetAsWeakPtr());
  action_view.reset();
  action_item->SetEnabled(true);
}

TEST_F(ActionViewControllerTest, TriggerAction) {
  std::unique_ptr<Widget> test_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* parent_view = test_widget->SetContentsView(std::make_unique<View>());
  MdTextButton* action_view =
      parent_view->AddChildView(std::make_unique<MdTextButton>());
  test_widget->Show();
  std::unique_ptr<actions::ActionItem> action_item = CreateEnabledActionItem();
  auto action_view_controller =
      std::make_unique<ActionViewControllerTemplate<MdTextButton>>(
          action_view, action_item->GetAsWeakPtr());
  action_view_controller->SetActionItem(action_item->GetAsWeakPtr());
  EXPECT_EQ(0, action_item->GetInvokeCount());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(action_view);
  test_api.NotifyClick(e);
  EXPECT_EQ(1, action_item->GetInvokeCount());
}

TEST_F(ActionViewControllerTest, TestCreateActionViewRelationship) {
  auto first_action_view = std::make_unique<MdTextButton>();
  auto second_action_view = std::make_unique<MdTextButton>();
  std::unique_ptr<actions::ActionItem> first_action_item =
      CreateEnabledActionItem();
  std::unique_ptr<actions::ActionItem> second_action_item =
      CreateDisabledActionItem();
  ActionViewController action_view_controller = ActionViewController();
  action_view_controller.CreateActionViewRelationship(
      first_action_view.get(), first_action_item->GetAsWeakPtr());
  action_view_controller.CreateActionViewRelationship(
      second_action_view.get(), second_action_item->GetAsWeakPtr());
  EXPECT_TRUE(first_action_view->GetEnabled());
  EXPECT_FALSE(second_action_view->GetEnabled());
  // View should respond to its action item changing.
  first_action_item->SetEnabled(false);
  EXPECT_FALSE(first_action_view->GetEnabled());
  // Change the action item, then modify the original action item and make sure
  // the view does not respond to the original action item being changed.
  action_view_controller.CreateActionViewRelationship(
      first_action_view.get(), second_action_item->GetAsWeakPtr());
  first_action_item->SetEnabled(true);
  EXPECT_FALSE(first_action_view->GetEnabled());
}

TEST_F(ActionViewControllerTest, TestActionInvocationContext) {
  std::unique_ptr<actions::ActionItem> action_item = CreateEnabledActionItem();
  auto invoke_action_callback = [](actions::ActionItem* action_item,
                                   actions::ActionInvocationContext context) {
    EXPECT_EQ(context.GetProperty(kTestPropertyValueKey), true);
  };
  action_item->SetInvokeActionCallback(
      base::BindRepeating(invoke_action_callback));

  std::unique_ptr<Widget> test_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* parent_view = test_widget->SetContentsView(std::make_unique<View>());
  TestActionButton* test_button =
      parent_view->AddChildView(std::make_unique<TestActionButton>());
  test_widget->Show();

  ActionViewController action_view_controller = ActionViewController();
  action_view_controller.CreateActionViewRelationship(
      test_button, action_item->GetAsWeakPtr());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(test_button);
  test_api.NotifyClick(e);
}

TEST_F(ActionViewControllerTest, TestOnViewChanged) {
  std::unique_ptr<Widget> test_widget =
      CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  View* parent_view = test_widget->SetContentsView(std::make_unique<View>());
  TestActionButton* test_view =
      parent_view->AddChildView(std::make_unique<TestActionButton>());
  test_widget->Show();

  auto action_view_controller = std::make_unique<ActionViewController>();
  std::unique_ptr<actions::ActionItem> action_item = CreateEnabledActionItem();

  EXPECT_TRUE(test_view->GetVisible());
  action_view_controller->CreateActionViewRelationship(
      test_view, action_item->GetAsWeakPtr());
  test_view->NotifyViewControllerCallback();
  EXPECT_FALSE(test_view->GetVisible());
}

}  // namespace views
