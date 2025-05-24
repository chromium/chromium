// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device_observer_ios.h"

#include "build/blink_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/input_device_event_observer.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace ui {

class MockInputDeviceEventObserver : public InputDeviceEventObserver {
 public:
  MOCK_METHOD(void, OnInputDeviceConfigurationChanged, (uint8_t), (override));
};

class InputDeviceObserverIOSTest : public testing::Test {
 protected:
  void SetUp() override { observer_ = InputDeviceObserverIOS::GetInstance(); }

  void TearDown() override {
    for (auto observer : added_observers_) {
      observer_->RemoveObserver(observer);
    }
  }

  raw_ptr<InputDeviceObserverIOS> observer_;
  std::vector<raw_ptr<MockInputDeviceEventObserver>> added_observers_;
};

TEST_F(InputDeviceObserverIOSTest, AddRemoveObserver) {
  MockInputDeviceEventObserver mock_observer;

  observer_->AddObserver(&mock_observer);
  added_observers_.push_back(&mock_observer);

  EXPECT_CALL(mock_observer, OnInputDeviceConfigurationChanged(
                                 InputDeviceEventObserver::kMouse))
      .Times(1);
  observer_->NotifyObserversDeviceConfigurationChanged(true);

  observer_->RemoveObserver(&mock_observer);
  added_observers_.pop_back();

  EXPECT_CALL(mock_observer, OnInputDeviceConfigurationChanged(
                                 InputDeviceEventObserver::kMouse))
      .Times(0);
  observer_->NotifyObserversDeviceConfigurationChanged(false);
}

TEST_F(InputDeviceObserverIOSTest, HasMouseDeviceState) {
  EXPECT_FALSE(observer_->GetHasMouseDevice());

  observer_->NotifyObserversDeviceConfigurationChanged(true);
  EXPECT_TRUE(observer_->GetHasMouseDevice());

  observer_->NotifyObserversDeviceConfigurationChanged(false);
  EXPECT_FALSE(observer_->GetHasMouseDevice());
}

TEST_F(InputDeviceObserverIOSTest, NotifyMultipleObservers) {
  MockInputDeviceEventObserver mock_observer1;
  MockInputDeviceEventObserver mock_observer2;

  observer_->AddObserver(&mock_observer1);
  observer_->AddObserver(&mock_observer2);
  added_observers_.push_back(&mock_observer1);
  added_observers_.push_back(&mock_observer2);

  EXPECT_CALL(mock_observer1, OnInputDeviceConfigurationChanged(
                                  InputDeviceEventObserver::kMouse))
      .Times(1);
  EXPECT_CALL(mock_observer2, OnInputDeviceConfigurationChanged(
                                  InputDeviceEventObserver::kMouse))
      .Times(1);
  observer_->NotifyObserversDeviceConfigurationChanged(true);
}

}  // namespace ui
