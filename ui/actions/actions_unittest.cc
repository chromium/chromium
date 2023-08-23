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

class ActionManagerTest : public testing::Test {
 public:
  ActionManagerTest() { ActionManager::ResetForTesting(); }
  ActionManagerTest(const ActionManagerTest&) = delete;
  ActionManagerTest& operator=(const ActionManagerTest&) = delete;
  ~ActionManagerTest() override { ActionManager::ResetForTesting(); }
};

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

}  // namespace actions
