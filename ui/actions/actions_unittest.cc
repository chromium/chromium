// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/actions/actions.h"

#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/class_property.h"

namespace actions {

enum class ContextValues {
  kContextNone,
  kContextKeyboard,
  kContextMouse,
  kContextTouch,
};

namespace {

const std::u16string kActionText = u"Test Action";
const std::u16string kChild1Text = u"Child Action 1";
const std::u16string kChild2Text = u"Child Action 2";
const std::u16string kActionAccessibleText = u"Accessible Action Text";
const std::u16string kActionTooltipText = u"Tooltip text";

#define TEST_ACTION_IDS                              \
  E(kActionTest1, , kActionTestStart, TestActionIds) \
  E(kActionTest2)                                    \
  E(kActionTest3)                                    \
  E(kActionTest4)

#include "ui/actions/action_id_macros.inc"

// clang-format off
enum TestActionIds : ActionId {
  kActionTestStart = kActionsEnd,

  TEST_ACTION_IDS

  kActionTestEnd,
};
// clang-format on

#include "ui/actions/action_id_macros.inc"

DEFINE_UI_CLASS_PROPERTY_KEY(ContextValues,
                             kContextValueKey,
                             ContextValues::kContextNone)

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

  void TearDown() override { ActionIdMap::ResetMapsForTesting(); }

 private:
  base::CallbackListSubscription initialization_subscription_;
};

using ActionItemTest = ActionManagerTest;
using ActionIdMapTest = ActionManagerTest;

}  // namespace
}  // namespace actions

DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(, actions::ContextValues)

DEFINE_UI_CLASS_PROPERTY_TYPE(actions::ContextValues)

namespace actions {

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
            [](int* invoked_count, actions::ActionItem* action,
               ActionInvocationContext context) {
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
  EXPECT_EQ(action->GetLastInvokeTime(), std::nullopt);
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

TEST_F(ActionIdMapTest, TestCreateActionId) {
  const std::string new_action_id_1 = "kNewActionId1";
  const std::string new_action_id_2 = "kNewActionId2";
  const std::string existing_action_id = "kActionPaste";

  auto result_1 = ActionIdMap::CreateActionId(new_action_id_1);
  EXPECT_TRUE(result_1.second);

  auto result_2 = ActionIdMap::CreateActionId(new_action_id_2);
  EXPECT_TRUE(result_2.second);
  EXPECT_NE(result_1.first, result_2.first);

  auto result_2_dupe = ActionIdMap::CreateActionId(new_action_id_2);
  EXPECT_FALSE(result_2_dupe.second);
  EXPECT_EQ(result_2.first, result_2_dupe.first);

  auto result_existing = ActionIdMap::CreateActionId(existing_action_id);
  EXPECT_FALSE(result_existing.second);
}

TEST_F(ActionIdMapTest, MapBetweenEnumAndString) {
  auto result_1 = ActionIdMap::CreateActionId("kNewActionId1");
  EXPECT_TRUE(result_1.second);

  const std::string expected_action_string = "kActionPaste";
  auto actual_action_string = ActionIdMap::ActionIdToString(kActionPaste);
  ASSERT_THAT(actual_action_string, testing::Optional(expected_action_string));

  // Map back from enum to string
  auto actual_action_id =
      ActionIdMap::StringToActionId(actual_action_string.value());
  EXPECT_THAT(actual_action_id, testing::Optional(kActionPaste));

  const std::vector<std::string> strings{"kActionPaste", "kActionCut"};
  const std::vector<ActionId> action_ids{kActionPaste, kActionCut};

  auto actual_strings = ActionIdMap::ActionIdsToStrings(action_ids);
  ASSERT_EQ(strings.size(), actual_strings.size());
  EXPECT_THAT(actual_strings[0], testing::Optional(strings[0]));
  EXPECT_THAT(actual_strings[1], testing::Optional(strings[1]));

  auto actual_action_ids = ActionIdMap::StringsToActionIds(strings);
  ASSERT_EQ(action_ids.size(), actual_action_ids.size());
  EXPECT_THAT(actual_action_ids[0], testing::Optional(action_ids[0]));
  EXPECT_THAT(actual_action_ids[1], testing::Optional(action_ids[1]));
}

#define MAP_ACTION_IDS_TO_STRINGS
#include "ui/actions/action_id_macros.inc"

TEST_F(ActionIdMapTest, MergeMaps) {
  auto test_action_map = base::MakeFlatMap<ActionId, std::string>(
      std::vector<std::pair<ActionId, std::string>>{TEST_ACTION_IDS});
  ActionIdMap::AddActionIdToStringMappings(test_action_map);

  const std::string expected_action_string = "kActionPaste";
  auto actual_action_string = ActionIdMap::ActionIdToString(kActionPaste);
  EXPECT_THAT(actual_action_string, testing::Optional(expected_action_string));

  const std::string expected_string = "kActionTest2";
  auto actual_string = ActionIdMap::ActionIdToString(kActionTest2);
  EXPECT_THAT(actual_string, testing::Optional(expected_string));
}

#include "ui/actions/action_id_macros.inc"
#undef MAP_ACTION_IDS_TO_STRINGS

TEST_F(ActionIdMapTest, TestEnumNotFound) {
  const std::string unknown_action = "kActionUnknown";
  auto unknown_id = ActionIdMap::StringToActionId(unknown_action);
  EXPECT_FALSE(unknown_id.has_value());

  const ActionId invalid_action_id = static_cast<ActionId>(-1);
  auto unknown_id_string = ActionIdMap::ActionIdToString(invalid_action_id);
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
      base::BindRepeating([](int* invoked_count, actions::ActionItem* action,
          ActionInvocationContext context){
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
              .SetIsShowingBubble(true)
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
  EXPECT_FALSE(child_action1->GetIsShowingBubble());

  EXPECT_EQ(child_action2->GetText(), kChild2Text);
  auto child_action_id2 = child_action2->GetActionId();
  ASSERT_TRUE(child_action_id2);
  EXPECT_EQ(child_action_id2.value(), kActionTest3);
  EXPECT_TRUE(child_action2->GetChecked());
  EXPECT_TRUE(child_action2->GetIsShowingBubble());

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

TEST_F(ActionItemTest, TestActionProperties) {
  constexpr int kGroupId = 5;
  // clang-format off
  auto builder = ActionItem::Builder()
      .SetText(kActionText)
      .SetActionId(kActionTest1)
      .SetVisible(true)
      .SetEnabled(true)
      .SetAccessibleName(kActionAccessibleText)
      .SetTooltipText(kActionTooltipText)
      .SetGroupId(kGroupId);
  // clang-format on
  auto action_item = std::move(builder).Build();
  EXPECT_EQ(action_item->GetText(), kActionText);
  EXPECT_EQ(action_item->GetActionId().value(), kActionTest1);
  EXPECT_TRUE(action_item->GetVisible());
  EXPECT_TRUE(action_item->GetEnabled());
  EXPECT_EQ(action_item->GetAccessibleName(), kActionAccessibleText);
  EXPECT_EQ(action_item->GetTooltipText(), kActionTooltipText);
  EXPECT_EQ(action_item->GetGroupId(), kGroupId);
}

TEST_F(ActionItemTest, TestActionWeakPtr) {
  base::WeakPtr<ActionItem> action_test2;
  base::WeakPtr<ActionItem> action_test3;
  // clang-format off
  auto builder = ActionItem::Builder()
      .SetText(kActionText)
      .SetActionId(kActionTest1)
      .SetVisible(true)
      .SetEnabled(false)
      .AddChildren(
          ActionItem::Builder()
              .CopyWeakPtrTo(&action_test2)
              .SetActionId(kActionTest2)
              .SetGroupId(10)
              .SetText(kChild1Text),
          ActionItem::Builder()
              .CopyWeakPtrTo(&action_test3)
              .SetActionId(kActionTest3)
              .SetGroupId(10)
              .SetChecked(true)
              .SetText(kChild2Text));
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddAction(std::move(builder).Build());
  ASSERT_NE(action_test2.get(), nullptr);
  ASSERT_NE(action_test3.get(), nullptr);
  manager.ResetActions();
  EXPECT_EQ(action_test2.get(), nullptr);
  EXPECT_EQ(action_test3.get(), nullptr);
}

TEST_F(ActionItemTest, TextActionInvocationContext) {
  int action_invoked_count = 0;
  base::WeakPtr<ActionItem> action_test1;
  // clang-format off
  auto builder = ActionItem::Builder(
      base::BindRepeating([](int* invoked_count, actions::ActionItem* action,
          ActionInvocationContext context){
        ++*invoked_count;
        ContextValues context_value = context.GetProperty(kContextValueKey);
        EXPECT_EQ(context_value, ContextValues::kContextKeyboard);
      },  &action_invoked_count))
      .CopyWeakPtrTo(&action_test1)
      .SetText(kActionText)
      .SetActionId(kActionTest1)
      .SetVisible(true)
      .SetEnabled(true);
  // clang-format on
  auto& manager = ActionManager::GetForTesting();
  manager.AddAction(std::move(builder).Build());
  ASSERT_TRUE(action_test1);
  action_test1->InvokeAction(
      ActionInvocationContext::Builder()
          .SetProperty(kContextValueKey, ContextValues::kContextKeyboard)
          .Build());
  EXPECT_EQ(action_invoked_count, 1);
}

}  // namespace actions
