// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_factory_evdev.h"

#include <optional>

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_property.h"
#include "ui/events/ozone/features.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace ui {

namespace {

class MockDeviceManager : public DeviceManager {
 public:
  MOCK_METHOD(void, ScanDevices, (DeviceEventObserver*));
  MOCK_METHOD(void, AddObserver, (DeviceEventObserver*));
  MOCK_METHOD(void, RemoveObserver, (DeviceEventObserver*));
};

class MockPlatformEventObserver : public PlatformEventObserver {
 public:
  MOCK_METHOD(void, WillProcessEvent, (const PlatformEvent& event));
  MOCK_METHOD(void, DidProcessEvent, (const PlatformEvent& event));
};

class EventFactoryEvdevTest : public testing::Test {
 protected:
  EventFactoryEvdevTest() : event_factory_(nullptr, &device_manager_, nullptr) {
    scoped_feature_list_.InitAndEnableFeature(kEnableOrdinalMotion);
    event_factory_.SetUserInputTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    event_factory_.Init();
    event_factory_.AddPlatformEventObserver(&event_observer_);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  testing::StrictMock<MockDeviceManager> device_manager_;
  testing::NiceMock<MockPlatformEventObserver> event_observer_;
  EventFactoryEvdev event_factory_;
};

TEST_F(EventFactoryEvdevTest, OrdinalImpliesFlag) {
  EXPECT_CALL(event_observer_, WillProcessEvent)
      .WillOnce([](const PlatformEvent& platform_event) {
        MouseEvent mouse_event(platform_event);
        EXPECT_TRUE(mouse_event.flags() & EF_UNADJUSTED_MOUSE);
        EXPECT_EQ(mouse_event.movement(), gfx::Vector2dF(2.67, 3.14));
      });

  gfx::Vector2dF ordinal_movement(2.67, 3.14);
  event_factory_.DispatchMouseMoveEvent(
      MouseMoveEventParams{0,
                           0,
                           {},
                           &ordinal_movement,
                           PointerDetails(EventPointerType::kMouse),
                           EventTimeForNow()});
}

TEST_F(EventFactoryEvdevTest, NoOrdinalImpliesNoFlag) {
  EXPECT_CALL(event_observer_, WillProcessEvent)
      .WillOnce([](const PlatformEvent& platform_event) {
        MouseEvent mouse_event(platform_event);
        EXPECT_FALSE(mouse_event.flags() & EF_UNADJUSTED_MOUSE);
      });

  event_factory_.DispatchMouseMoveEvent(
      MouseMoveEventParams{0,
                           0,
                           {},
                           /*ordinal_movement=*/nullptr,
                           PointerDetails(EventPointerType::kMouse),
                           EventTimeForNow()});
}

TEST_F(EventFactoryEvdevTest, MouseSideButtonRemapsToBack) {
  EXPECT_CALL(event_observer_, WillProcessEvent)
      .WillOnce([](const PlatformEvent& platform_event) {
        MouseEvent mouse_event(platform_event);
        EXPECT_TRUE(mouse_event.changed_button_flags() & EF_BACK_MOUSE_BUTTON);
        auto button = GetForwardBackMouseButtonProperty(mouse_event);
        ASSERT_TRUE(button);
        EXPECT_EQ(static_cast<uint32_t>(BTN_SIDE), *button);
      });

  event_factory_.DispatchMouseButtonEvent(MouseButtonEventParams{
      0, 0, gfx::PointF{}, BTN_SIDE, true, MouseButtonMapType::kMouse,
      PointerDetails(EventPointerType::kMouse), EventTimeForNow()});
}

TEST_F(EventFactoryEvdevTest, MouseBackButtonRemapsToBack) {
  EXPECT_CALL(event_observer_, WillProcessEvent)
      .WillOnce([](const PlatformEvent& platform_event) {
        MouseEvent mouse_event(platform_event);
        EXPECT_TRUE(mouse_event.changed_button_flags() & EF_BACK_MOUSE_BUTTON);
        auto button = GetForwardBackMouseButtonProperty(mouse_event);
        ASSERT_TRUE(button);
        EXPECT_EQ(static_cast<uint32_t>(BTN_BACK), *button);
      });

  event_factory_.DispatchMouseButtonEvent(MouseButtonEventParams{
      0, 0, gfx::PointF{}, BTN_BACK, true, MouseButtonMapType::kMouse,
      PointerDetails(EventPointerType::kMouse), EventTimeForNow()});
}

TEST_F(EventFactoryEvdevTest, MouseExtraButtonRemapsToForward) {
  EXPECT_CALL(event_observer_, WillProcessEvent)
      .WillOnce([](const PlatformEvent& platform_event) {
        MouseEvent mouse_event(platform_event);
        EXPECT_TRUE(mouse_event.changed_button_flags() &
                    EF_FORWARD_MOUSE_BUTTON);
        auto button = GetForwardBackMouseButtonProperty(mouse_event);
        ASSERT_TRUE(button);
        EXPECT_EQ(static_cast<uint32_t>(BTN_EXTRA), *button);
      });

  event_factory_.DispatchMouseButtonEvent(MouseButtonEventParams{
      0, 0, gfx::PointF{}, BTN_EXTRA, true, MouseButtonMapType::kMouse,
      PointerDetails(EventPointerType::kMouse), EventTimeForNow()});
}

TEST_F(EventFactoryEvdevTest, MouseForwardButtonRemapsToForward) {
  EXPECT_CALL(event_observer_, WillProcessEvent)
      .WillOnce([](const PlatformEvent& platform_event) {
        MouseEvent mouse_event(platform_event);
        EXPECT_TRUE(mouse_event.changed_button_flags() &
                    EF_FORWARD_MOUSE_BUTTON);
        auto button = GetForwardBackMouseButtonProperty(mouse_event);
        ASSERT_TRUE(button);
        EXPECT_EQ(static_cast<uint32_t>(BTN_FORWARD), *button);
      });

  event_factory_.DispatchMouseButtonEvent(MouseButtonEventParams{
      0, 0, gfx::PointF{}, BTN_FORWARD, true, MouseButtonMapType::kMouse,
      PointerDetails(EventPointerType::kMouse), EventTimeForNow()});
}

}  // namespace
}  // namespace ui
