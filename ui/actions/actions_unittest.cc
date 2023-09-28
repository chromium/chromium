// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/actions/actions.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/class_property.h"

namespace actions {

namespace {

const std::u16string kActionText = u"Test Action";
const std::u16string kChild1Text = u"Child Action 1";
const std::u16string kChild2Text = u"Child Action 2";

#define TEST_ACTION_IDS                              \
  E(kActionTest1, , kActionTestStart, TestActionIds) \
  E(kActionTest2)                                    \
  E(kActionTest3)                                    \
  E(kActionTest4)

#include "ui/actions/action_id_macros.inc"

enum TestActionIds : ActionId {
  kActionTestStart = kActionsEnd,

  TEST_ACTION_IDS

      kActionTestEnd,
};

#include "ui/actions/action_id_macros.inc"

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
    initialization_subscription_ =
        manager.AppendActionItemInitializer(base::BindRepeating(
            &ActionManagerTest::InitializeActions, base::Unretained(this)));
  }

 private:
  base::CallbackListSubscription initialization_subscription_;
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
  auto subscription = manager.AppendActionItemInitializer(base::BindRepeating(
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
  auto subscription = manager.AppendActionItemInitializer(base::BindRepeating(
      [](int* invoked_count, const std::u16string& text,
         ActionManager* manager) {
        auto action = std::make_unique<ActionItem>(base::BindRepeating(
            [](int* invoked_count, actions::ActionItem* action) {
              ++*invoked_count;
              EXPECT_EQ(*invoked_count, action->GetInvokeCount());
              EXPECT_GE(base::TimeTicks::Now(), *action->GetLastInvokeTime());
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
  EXPECT_EQ(action->GetInvokeCount(), 0);
  EXPECT_EQ(action->GetLastInvokeTime(), absl::nullopt);
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

TEST_F(ActionManagerTest, TestCreateActionId) {
  const std::string new_action_id_1 = "kNewActionId1";
  const std::string new_action_id_2 = "kNewActionId2";
  const std::string existing_action_id = "kActionPaste";

  auto result_1 = ActionManager::CreateActionId(new_action_id_1);
  EXPECT_TRUE(result_1.second);

  auto result_2 = ActionManager::CreateActionId(new_action_id_2);
  EXPECT_TRUE(result_2.second);
  EXPECT_NE(result_1.first, result_2.first);

  auto result_2_dupe = ActionManager::CreateActionId(new_action_id_2);
  EXPECT_FALSE(result_2_dupe.second);
  EXPECT_EQ(result_2.first, result_2_dupe.first);

  auto result_existing = ActionManager::CreateActionId(existing_action_id);
  EXPECT_FALSE(result_existing.second);
}

TEST_F(ActionManagerTest, MapBetweenEnumAndString) {
  const std::string expected_action_string = "kActionPaste";
  auto actual_action_string = ActionManager::ActionIdToString(kActionPaste);
  ASSERT_TRUE(actual_action_string.has_value());
  EXPECT_EQ(expected_action_string, actual_action_string.value());

  // Map back from enum to string
  const ActionId expected_action_id = kActionPaste;
  auto actual_action_id =
      ActionManager::StringToActionId(actual_action_string.value());
  ASSERT_TRUE(actual_action_id.has_value());
  EXPECT_EQ(expected_action_id, actual_action_id.value());

  const std::vector<std::string> strings{"kActionPaste", "kActionCut"};
  const std::vector<ActionId> action_ids{kActionPaste, kActionCut};

  auto actual_strings = ActionManager::ActionIdsToStrings(action_ids);
  EXPECT_EQ(strings.size(), actual_strings.size());
  EXPECT_EQ(strings[0], actual_strings[0].value());
  EXPECT_EQ(strings[1], actual_strings[1].value());

  auto actual_action_ids = ActionManager::StringsToActionIds(strings);
  EXPECT_EQ(action_ids.size(), actual_action_ids.size());
  EXPECT_EQ(action_ids[0], actual_action_ids[0].value());
  EXPECT_EQ(action_ids[1], actual_action_ids[1].value());
}

#define MAP_ACTION_IDS_TO_STRINGS
#include "ui/actions/action_id_macros.inc"

TEST_F(ActionManagerTest, MergeMaps) {
  auto kTestActionMap = base::MakeFlatMap<ActionId, std::string_view>(
      std::vector<std::pair<ActionId, std::string_view>>{TEST_ACTION_IDS});
  ActionManager::AddActionIdToStringMappings(kTestActionMap);

  const std::string expected_action_string = "kActionPaste";
  auto actual_action_string = ActionManager::ActionIdToString(kActionPaste);
  ASSERT_TRUE(actual_action_string.has_value());
  EXPECT_EQ(expected_action_string, actual_action_string.value());

  const std::string expected_string = "kActionTest2";
  auto actual_string = ActionManager::ActionIdToString(kActionTest2);
  ASSERT_TRUE(actual_string.has_value());
  EXPECT_EQ(expected_string, actual_string.value());
}

#include "ui/actions/action_id_macros.inc"
#undef MAP_ACTION_IDS_TO_STRINGS

TEST_F(ActionManagerTest, TestEnumNotFound) {
  const std::string unknown_action = "kActionUnknown";
  auto unknown_id = ActionManager::StringToActionId(unknown_action);
  EXPECT_FALSE(unknown_id.has_value());

  const ActionId invalid_action_id = static_cast<ActionId>(-1);
  auto unknown_id_string = ActionManager::ActionIdToString(invalid_action_id);
  EXPECT_FALSE(unknown_id_string.has_value());
}

TEST_F(ActionItemTest, ActionBuilderTest) {
  const std::u16string text = u"Test Action";
  // clang-format off
  auto builder =
      ActionItem::Builder()
        .SetText(text)
        .SetVisible(false)
        .SetActionId(actions::kActionCopy)
        .SetInvokeActionCallback(
            base::DoNothing());
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddAction(std::move(builder).Build());

  auto* action = manager.FindAction(actions::kActionCopy);
  ASSERT_TRUE(action);
  EXPECT_EQ(action->GetText(), text);
  EXPECT_FALSE(action->GetVisible());
  EXPECT_EQ(action->GetInvokeCount(), 0);
  action->InvokeAction();
  EXPECT_EQ(action->GetInvokeCount(), 1);
}

TEST_F(ActionItemTest, ActionBuilderChildrenTest) {
  const size_t expected_child_count = 2;
  ActionItem* root_action = nullptr;
  ActionItem* child_action1 = nullptr;
  ActionItem* child_action2 = nullptr;
  int action_invoked_count = 0;
  // clang-format off
  auto builder = ActionItem::Builder(
      base::BindRepeating([](int* invoked_count, actions::ActionItem* action){
        ++*invoked_count;
      }, &action_invoked_count))
      .CopyAddressTo(&root_action)
      .SetText(kActionText)
      .SetActionId(kActionTest1)
      .SetVisible(true)
      .SetEnabled(false)
      .AddChildren(
          ActionItem::Builder(base::DoNothing())
              .CopyAddressTo(&child_action1)
              .SetActionId(kActionTest2)
              .SetText(kChild1Text),
          ActionItem::Builder(base::DoNothing())
              .CopyAddressTo(&child_action2)
              .SetActionId(kActionTest3)
              .SetChecked(true)
              .SetText(kChild2Text));
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddAction(std::move(builder).Build());
  ASSERT_TRUE(root_action);
  ASSERT_TRUE(child_action1);
  ASSERT_TRUE(child_action2);

  EXPECT_EQ(root_action->GetChildren().children().size(), expected_child_count);

  EXPECT_EQ(child_action1->GetText(), kChild1Text);
  auto child_action_id1 = child_action1->GetActionId();
  ASSERT_TRUE(child_action_id1);
  EXPECT_EQ(child_action_id1.value(), kActionTest2);
  EXPECT_FALSE(child_action1->GetChecked());

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
  EXPECT_EQ(child_action1->GetInvokeCount(), 1);
  child_action2->InvokeAction();
  EXPECT_EQ(child_action2->GetInvokeCount(), 1);
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
  ActionItem* root_action = nullptr;
  // clang-format off
  auto builder = ActionItem::Builder()
      .CopyAddressTo(&root_action)
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
  ActionItem* action_test2 = nullptr;
  ActionItem* action_test3 = nullptr;
  // clang-format off
  auto builder = ActionItem::Builder()
      .SetText(kActionText)
      .SetActionId(kActionTest1)
      .SetVisible(true)
      .SetEnabled(false)
      .AddChildren(
          ActionItem::Builder()
              .CopyAddressTo(&action_test2)
              .SetActionId(kActionTest2)
              .SetGroupId(10)
              .SetText(kChild1Text),
          ActionItem::Builder()
              .CopyAddressTo(&action_test3)
              .SetActionId(kActionTest3)
              .SetGroupId(10)
              .SetChecked(true)
              .SetText(kChild2Text));
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddActions(std::move(builder).Build(),
                     ActionItem::Builder().SetActionId(kActionTest4).Build());
  ASSERT_TRUE(action_test2);
  ASSERT_TRUE(action_test2);
  EXPECT_FALSE(action_test2->GetChecked());
  EXPECT_TRUE(action_test3->GetChecked());
  action_test2->SetChecked(true);
  EXPECT_TRUE(action_test2->GetChecked());
  EXPECT_FALSE(action_test3->GetChecked());
}

TEST_F(ActionItemTest, TestActionItemPinnableKey) {
  // clang-format off
  auto builder = ActionItem::Builder()
      .SetText(kActionText)
      .SetActionId(kActionTest1)
      .SetVisible(true)
      .SetEnabled(true);
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddAction(std::move(builder).Build());
  auto* action_test1 = manager.FindAction(kActionTest1);
  ASSERT_TRUE(action_test1);
  ASSERT_FALSE(action_test1->GetProperty(kActionItemPinnableKey));
  action_test1->SetProperty(kActionItemPinnableKey, true);
  ASSERT_TRUE(action_test1->GetProperty(kActionItemPinnableKey));

  // test using builder
  builder = ActionItem::Builder()
                .SetText(kActionText)
                .SetActionId(kActionTest2)
                .SetProperty(kActionItemPinnableKey, true)
                .SetVisible(true)
                .SetEnabled(true);

  manager.AddAction(std::move(builder).Build());
  auto* action_test2 = manager.FindAction(kActionTest2);
  ASSERT_TRUE(action_test2);
  ASSERT_TRUE(action_test2->GetProperty(kActionItemPinnableKey));
}

}  // namespace actions
