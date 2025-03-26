// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
namespace {

// Test implementation of GlobalAcceleratorListener that doesn't delegate
// to an OS-specific implementation. All this does is fail to register an
// accelerator if it has already been registered.
class BaseGlobalAcceleratorListenerForTesting final
    : public ui::GlobalAcceleratorListener {
 public:
  void StartListening() override {}
  void StopListening() override {}

  bool StartListeningForAccelerator(
      const ui::Accelerator& accelerator) override {
    if (registered_accelerators_.contains(accelerator)) {
      return false;
    }

    registered_accelerators_.insert(accelerator);
    return true;
  }

  void StopListeningForAccelerator(
      const ui::Accelerator& accelerator) override {
    registered_accelerators_.erase(accelerator);
  }

  MOCK_CONST_METHOD0(IsRegistrationHandledExternally, bool());
  MOCK_METHOD4(OnCommandsChanged,
               void(const std::string&,
                    const std::string&,
                    const ui::CommandMap&,
                    Observer*));

 private:
  std::set<ui::Accelerator> registered_accelerators_;
};

class TestObserver final : public GlobalAcceleratorListener::Observer {
 public:
  ~TestObserver() = default;

  void OnKeyPressed(const ui::Accelerator& accelerator) override {}

  void ExecuteCommand(const std::string& accelerator_group_id,
                      const std::string& command_id) override {}
};

class GlobalAcceleratorListenerTest : public testing::Test {
 public:
  GlobalAcceleratorListenerTest() {
    ui_listener_ = std::make_unique<BaseGlobalAcceleratorListenerForTesting>();
  }

  GlobalAcceleratorListenerTest(const GlobalAcceleratorListenerTest&) = delete;
  GlobalAcceleratorListenerTest& operator=(
      const GlobalAcceleratorListenerTest&) = delete;

  void SetUp() override {
    ui_listener_->SetShortcutHandlingSuspended(false);
    observer_ = std::make_unique<TestObserver>();
  }
  void TearDown() override {
    observer_ = nullptr;
    ui_listener_ = nullptr;
  }

  GlobalAcceleratorListener::Observer* GetObserver() { return observer_.get(); }

  BaseGlobalAcceleratorListenerForTesting* GetUIListener() {
    return ui_listener_.get();
  }

 private:
  std::unique_ptr<BaseGlobalAcceleratorListenerForTesting> ui_listener_;
  std::unique_ptr<TestObserver> observer_ = nullptr;
};

TEST_F(GlobalAcceleratorListenerTest, RegistersAccelerators) {
  GlobalAcceleratorListener* listener = GetUIListener();
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);

  // First registration attempt succeeds.
  EXPECT_TRUE(listener->RegisterAccelerator(accelerator_a, GetObserver()));

  // A second registration fails because the accelerator is already registered.
  EXPECT_FALSE(listener->RegisterAccelerator(accelerator_a, GetObserver()));

  // Clean up registration.
  listener->UnregisterAccelerator(accelerator_a, GetObserver());
}

TEST_F(GlobalAcceleratorListenerTest, SuspendsShortcutHandling) {
  GlobalAcceleratorListener* listener = GetUIListener();
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);

  listener->SetShortcutHandlingSuspended(true);
  EXPECT_TRUE(listener->IsShortcutHandlingSuspended());

  // Can't register accelerator while shortcut handling is suspended.
  EXPECT_FALSE(listener->RegisterAccelerator(accelerator_b, GetObserver()));

  listener->SetShortcutHandlingSuspended(false);
  EXPECT_FALSE(listener->IsShortcutHandlingSuspended());

  // Can register accelerator when shortcut handling isn't suspended.
  EXPECT_TRUE(listener->RegisterAccelerator(accelerator_b, GetObserver()));

  // Clean up registration.
  listener->UnregisterAccelerator(accelerator_b, GetObserver());
}

TEST_F(GlobalAcceleratorListenerTest, IsRegistrationHandledExternally) {
  GlobalAcceleratorListener* listener = GetUIListener();
  BaseGlobalAcceleratorListenerForTesting* ui_listener = GetUIListener();

  EXPECT_CALL(*ui_listener, IsRegistrationHandledExternally())
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(listener->IsRegistrationHandledExternally());
}

TEST_F(GlobalAcceleratorListenerTest, OnCommandsChanged) {
  GlobalAcceleratorListener* listener = GetUIListener();
  BaseGlobalAcceleratorListenerForTesting* ui_listener = GetUIListener();

  const std::string kAcceleratorGroupId = "group_id";
  const std::string kProfileId = "profile_id";
  const ui::CommandMap kCommands;
  EXPECT_CALL(*ui_listener, OnCommandsChanged(kAcceleratorGroupId, kProfileId,
                                              testing::_, testing::_));
  listener->OnCommandsChanged(kAcceleratorGroupId, kProfileId, kCommands,
                              GetObserver());
}

}  // namespace
}  // namespace ui
