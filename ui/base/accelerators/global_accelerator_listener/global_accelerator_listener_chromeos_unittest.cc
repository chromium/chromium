// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener_chromeos.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
namespace {

class TestObserver final : public GlobalAcceleratorListener::Observer {
 public:
  void OnKeyPressed(const ui::Accelerator& accelerator) override {}
  void ExecuteCommand(const std::string& accelerator_group_id,
                      const std::string& command_id) override {}
};

class FakeDelegate : public GlobalAcceleratorListenerChromeOS::Delegate {
 public:
  FakeDelegate() = default;
  ~FakeDelegate() override = default;

  bool IsReserved(const Accelerator& accelerator) const override {
    return is_reserved_;
  }

  bool IsRegistered(const Accelerator& accelerator) const override {
    return is_registered_;
  }

  void Register(const std::vector<Accelerator>& accelerators,
                AcceleratorTarget* target) override {
    registered_count_++;
    last_registered_ = accelerators;
    last_target_ = target;
  }

  void Unregister(const Accelerator& accelerator,
                  AcceleratorTarget* target) override {
    unregistered_count_++;
    last_unregistered_ = accelerator;
    last_target_ = target;
  }

  void UnregisterAll(AcceleratorTarget* target) override {
    unregister_all_count_++;
    last_target_ = target;
  }

  bool is_reserved_ = false;
  bool is_registered_ = false;
  int registered_count_ = 0;
  int unregistered_count_ = 0;
  int unregister_all_count_ = 0;
  std::vector<Accelerator> last_registered_;
  Accelerator last_unregistered_;
  raw_ptr<AcceleratorTarget, DisableDanglingPtrDetection> last_target_ =
      nullptr;
};

class GlobalAcceleratorListenerChromeOSTest : public testing::Test {
 public:
  GlobalAcceleratorListenerChromeOSTest() = default;
  ~GlobalAcceleratorListenerChromeOSTest() override = default;

  void TearDown() override {
    GlobalAcceleratorListenerChromeOS::SetDelegate(nullptr);
  }

 protected:
  TestObserver observer_;
};

using GlobalAcceleratorListenerChromeOSDeathTest =
    GlobalAcceleratorListenerChromeOSTest;

TEST_F(GlobalAcceleratorListenerChromeOSDeathTest, WithoutDelegate) {
  GlobalAcceleratorListenerChromeOS::SetDelegate(nullptr);
  auto listener = GlobalAcceleratorListenerChromeOS::Create();
  const Accelerator accelerator(VKEY_A, EF_NONE);

  EXPECT_DEATH(listener->RegisterAccelerator(accelerator, &observer_), "");
}

TEST_F(GlobalAcceleratorListenerChromeOSTest,
       WithDelegateRegisterAndUnregister) {
  FakeDelegate delegate;
  GlobalAcceleratorListenerChromeOS::SetDelegate(&delegate);

  auto listener = GlobalAcceleratorListenerChromeOS::Create();
  const Accelerator accelerator(VKEY_A, EF_NONE);

  EXPECT_EQ(delegate.registered_count_, 0);
  EXPECT_TRUE(listener->RegisterAccelerator(accelerator, &observer_));
  EXPECT_EQ(delegate.registered_count_, 1);
  ASSERT_EQ(delegate.last_registered_.size(), 1u);
  EXPECT_EQ(delegate.last_registered_[0], accelerator);

  listener->UnregisterAccelerator(accelerator, &observer_);
  EXPECT_EQ(delegate.unregistered_count_, 1);
  EXPECT_EQ(delegate.last_unregistered_, accelerator);
}

TEST_F(GlobalAcceleratorListenerChromeOSTest,
       ReservedAndRegisteredAccelerator) {
  FakeDelegate delegate;
  delegate.is_reserved_ = true;
  delegate.is_registered_ = true;
  GlobalAcceleratorListenerChromeOS::SetDelegate(&delegate);

  auto listener = GlobalAcceleratorListenerChromeOS::Create();
  const Accelerator accelerator(VKEY_A, EF_NONE);

  EXPECT_FALSE(listener->RegisterAccelerator(accelerator, &observer_));
  EXPECT_EQ(delegate.registered_count_, 0);
}

TEST_F(GlobalAcceleratorListenerChromeOSTest, UnregisterAllOnDestruction) {
  FakeDelegate delegate;
  GlobalAcceleratorListenerChromeOS::SetDelegate(&delegate);

  {
    std::unique_ptr<GlobalAcceleratorListener> listener =
        GlobalAcceleratorListenerChromeOS::Create();
    EXPECT_EQ(delegate.unregister_all_count_, 0);
  }
  EXPECT_EQ(delegate.unregister_all_count_, 1);
}

}  // namespace
}  // namespace ui
