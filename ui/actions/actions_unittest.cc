// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/actions/actions.h"

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace actions {

namespace {

const std::u16string kActionText = u"Test Action";
const std::u16string kChild1Text = u"Child Action 1";
const std::u16string kChild2Text = u"Child Action 2";

enum TestActionIds : ActionId {
  kActionTestStart = kActionsStart,
  kActionTest1 = kActionTestStart,
  kActionTest2,
  kActionTest3,
  kActionTest4,
  kActionTestEnd,
};

class ActionManagerTest : public testing::Test {
 public:
  ActionManagerTest() { ActionManager::ResetForTesting(); }
  ActionManagerTest(const ActionManagerTest&) = delete;
  ActionManagerTest& operator=(const ActionManagerTest&) = delete;
  ~ActionManagerTest() override { ActionManager::ResetForTesting(); }

 protected:
  void InitializeActions(ActionManager* manager) {
    // clang-format off
    manager->AddAction(ActionItem::Builder()
        .SetText(kActionText)
        .SetActionId(kActionTest1)
        .SetVisible(true)
        .SetEnabled(false)
        .AddChildren(
            ActionItem::Builder()
                .SetActionId(kActionTest2)
                .SetText(kChild1Text),
            ActionItem::Builder()
                .SetActionId(kActionTest3)
                .SetText(kChild2Text)).Build());
    // clang-format on
  }
  void SetupInitializer() {
    auto& manager = ActionManager::GetForTesting();
    manager.AppendActionItemInitializer(base::BindRepeating(
        &ActionManagerTest::InitializeActions, base::Unretained(this)));
  }
};

using ActionItemTest = ActionManagerTest;

}  // namespace

// Verifies that the test harness functions correctly.
TEST_F(ActionManagerTest, Harness) {
  auto* manager = &ActionManager::GetForTesting();
  ActionManager::ResetForTesting();
  auto* new_manager = &ActionManager::GetForTesting();
  EXPECT_EQ(manager, new_manager);
}

// Verifies that the Initializers are properly called.
TEST_F(ActionManagerTest, InitializerTest) {
  bool initializer_called = false;
  auto& manager = ActionManager::GetForTesting();
  manager.AppendActionItemInitializer(base::BindRepeating(
      [](bool* called, ActionManager* manager) { *called = true; },
      &initializer_called));
  EXPECT_FALSE(initializer_called);
  manager.IndexActions();
  EXPECT_TRUE(initializer_called);
}

TEST_F(ActionManagerTest, ActionRegisterAndInvoke) {
  const std::u16string text = u"Test Action";
  int action_invoked_count = 0;
  auto& manager = ActionManager::GetForTesting();
  manager.AppendActionItemInitializer(base::BindRepeating(
      [](int* invoked_count, const std::u16string& text,
         ActionManager* manager) {
        auto action = std::make_unique<ActionItem>(base::BindRepeating(
            [](int* invoked_count, actions::ActionItem* action) {
              ++*invoked_count;
            },
            invoked_count));
        action->SetActionId(kActionCut);
        action->SetText(text);
        action->SetEnabled(true);
        action->SetVisible(true);
        manager->AddAction(std::move(action));
      },
      &action_invoked_count, text));
  EXPECT_EQ(action_invoked_count, 0);
  auto* action = manager.FindAction(kActionCut);
  ASSERT_TRUE(action);
  EXPECT_EQ(action->GetText(), text);
  EXPECT_TRUE(action->GetEnabled());
  EXPECT_TRUE(action->GetVisible());
  auto action_id = action->GetActionId();
  ASSERT_TRUE(action_id);
  EXPECT_EQ(*action_id, kActionCut);
  action->InvokeAction();
  EXPECT_GT(action_invoked_count, 0);
}

TEST_F(ActionManagerTest, ActionNotFound) {
  auto& manager = ActionManager::GetForTesting();
  auto* action = manager.FindAction(kActionPaste);
  EXPECT_FALSE(action);
}

TEST_F(ActionItemTest, ScopedFindActionTest) {
  // clang-format off
  auto builder = ActionItem::Builder()
      .SetText(kActionText)
      .SetActionId(kActionTest1)
      .SetVisible(true)
      .SetEnabled(false)
      .AddChildren(
          ActionItem::Builder()
              .SetActionId(kActionTest2)
              .SetText(kChild1Text),
          ActionItem::Builder()
              .SetActionId(kActionTest3)
              .SetChecked(true)
              .SetText(kChild2Text));
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddAction(std::move(builder).Build());
  auto* action_test1 = manager.FindAction(kActionTest1);
  ASSERT_TRUE(action_test1);
  auto* action_test2 = manager.FindAction(kActionTest2, action_test1);
  ASSERT_TRUE(action_test2);
  auto* action_test3 = manager.FindAction(kActionTest3, action_test2);
  EXPECT_FALSE(action_test3);
}

TEST_F(ActionItemTest, ActionBuilderTest) {
  const std::u16string text = u"Test Action";
  int action_invoked_count = 0;
  // clang-format off
  auto builder =
      ActionItem::Builder()
        .SetText(text)
        .SetVisible(false)
        .SetActionId(actions::kActionCopy)
        .SetInvokeActionCallback(
            base::BindRepeating(
                [](int* invoked_count, actions::ActionItem* action) {
                  ++*invoked_count;
                }, &action_invoked_count));
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddAction(std::move(builder).Build());

  auto* action = manager.FindAction(actions::kActionCopy);
  ASSERT_TRUE(action);
  EXPECT_EQ(action->GetText(), text);
  EXPECT_FALSE(action->GetVisible());
  EXPECT_EQ(action_invoked_count, 0);
  action->InvokeAction();
  EXPECT_EQ(action_invoked_count, 1);
}

TEST_F(ActionItemTest, ActionBuilderChildrenTest) {
  const size_t expected_child_count = 2;
  int action_invoked_count = 0;
  int child1_action_invoked_count = 0;
  int child2_action_invoked_count = 0;
  // clang-format off
  auto builder = ActionItem::Builder(
      base::BindRepeating([](int* invoked_count, actions::ActionItem* action){
        ++*invoked_count;
      }, &action_invoked_count))
      .SetText(kActionText)
      .SetActionId(kActionTest1)
      .SetVisible(true)
      .SetEnabled(false)
      .AddChildren(
          ActionItem::Builder(
              base::BindRepeating([](int* invoked_count,
                                    actions::ActionItem* action) {
                ++*invoked_count;
              }, &child1_action_invoked_count))
              .SetActionId(kActionTest2)
              .SetText(kChild1Text),
          ActionItem::Builder(
              base::BindRepeating([](int* invoked_count,
                                    actions::ActionItem* action) {
                ++*invoked_count;
              }, &child2_action_invoked_count))
              .SetActionId(kActionTest3)
              .SetChecked(true)
              .SetText(kChild2Text));
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddAction(std::move(builder).Build());
  auto* root_action = manager.FindAction(kActionTest1);
  EXPECT_EQ(root_action->GetChildren().children().size(), expected_child_count);
  // TODO(kylixrd): Once ActionManger::IndexActions() indexes the child actions,
  // FindAction() can be used to locate a child action. Go right to it for now.
  auto* child_action1 = root_action->GetChildren().children()[0].get();
  EXPECT_EQ(child_action1->GetText(), kChild1Text);
  auto child_action_id1 = child_action1->GetActionId();
  ASSERT_TRUE(child_action_id1);
  EXPECT_EQ(child_action_id1.value(), kActionTest2);
  EXPECT_FALSE(child_action1->GetChecked());

  auto* child_action2 = root_action->GetChildren().children()[1].get();
  EXPECT_EQ(child_action2->GetText(), kChild2Text);
  auto child_action_id2 = child_action2->GetActionId();
  ASSERT_TRUE(child_action_id2);
  EXPECT_EQ(child_action_id2.value(), kActionTest3);
  EXPECT_TRUE(child_action2->GetChecked());

  EXPECT_FALSE(root_action->GetEnabled());
  EXPECT_EQ(action_invoked_count, 0);
  root_action->InvokeAction();
  // `root_action` is not enabled, so InvokeAction() shouldn't trigger callback.
  EXPECT_EQ(action_invoked_count, 0);

  // The child actions should trigger their callbacks since they're enabled.
  child_action1->InvokeAction();
  EXPECT_EQ(child1_action_invoked_count, 1);
  child_action2->InvokeAction();
  EXPECT_EQ(child2_action_invoked_count, 1);
}

TEST_F(ActionItemTest, TestGetChildren) {
  ActionItemVector actions;
  auto& manager = ActionManager::GetForTesting();
  SetupInitializer();
  manager.GetActions(actions);
  EXPECT_FALSE(actions.empty());
  EXPECT_EQ(actions.size(), size_t{3});
}

TEST_F(ActionItemTest, TestItemBatchUpdate) {
  bool action_item_changed = false;
  // clang-format off
  auto builder = ActionItem::Builder()
      .SetText(kActionText)
      .SetActionId(kActionTest1)
      .SetVisible(true)
      .SetEnabled(false)
      .AddChildren(
          ActionItem::Builder()
              .SetActionId(kActionTest2)
              .SetText(kChild1Text),
          ActionItem::Builder()
              .SetActionId(kActionTest3)
              .SetChecked(true)
              .SetText(kChild2Text));
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddAction(std::move(builder).Build());
  auto* root_action = manager.FindAction(kActionTest1);
  auto changed_subscription =
      root_action->AddActionChangedCallback(base::BindRepeating(
          [](bool* action_item_changed) { *action_item_changed = true; },
          &action_item_changed));
  {
    auto scoped_updater = root_action->BeginUpdate();
    root_action->SetEnabled(true);
    EXPECT_FALSE(action_item_changed);
    root_action->SetVisible(false);
    EXPECT_FALSE(action_item_changed);
  }
  EXPECT_TRUE(action_item_changed);
}

TEST_F(ActionItemTest, TestGroupIdExclusion) {
  // clang-format off
  auto builder = ActionItem::Builder()
      .SetText(kActionText)
      .SetActionId(kActionTest1)
      .SetVisible(true)
      .SetEnabled(false)
      .AddChildren(
          ActionItem::Builder()
              .SetActionId(kActionTest2)
              .SetGroupId(10)
              .SetText(kChild1Text),
          ActionItem::Builder()
              .SetActionId(kActionTest3)
              .SetGroupId(10)
              .SetChecked(true)
              .SetText(kChild2Text));
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddAction(std::move(builder).Build());
  auto* action_test2 = manager.FindAction(kActionTest2);
  auto* action_test3 = manager.FindAction(kActionTest3);
  ASSERT_TRUE(action_test2);
  ASSERT_TRUE(action_test2);
  EXPECT_FALSE(action_test2->GetChecked());
  EXPECT_TRUE(action_test3->GetChecked());
  action_test2->SetChecked(true);
  EXPECT_TRUE(action_test2->GetChecked());
  EXPECT_FALSE(action_test3->GetChecked());
}

}  // namespace actions
