// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_factory_evdev.h"

#include <linux/input.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/safety_checks.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event_constants.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_property.h"
#include "ui/events/ozone/features.h"
#include "ui/events/ozone/gamepad/gamepad_event.h"
#include "ui/events/ozone/gamepad/gamepad_observer.h"
#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform_event.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED) &&           \
    !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
#include "partition_alloc/scheduler_loop_quarantine_support.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED) &&
        // !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)

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
    event_factory_.SetUserInputTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    event_factory_.Init();
    event_factory_.AddPlatformEventObserver(&event_observer_);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::StrictMock<MockDeviceManager> device_manager_;
  testing::NiceMock<MockPlatformEventObserver> event_observer_;
  EventFactoryEvdev event_factory_;
};

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

class MockGamepadObserver : public GamepadObserver {
 public:
  MOCK_METHOD(void, OnGamepadEvent, (const GamepadEvent& event), (override));
};

class EventFactoryEvdevSafetyChecksTest : public testing::TestWithParam<bool> {
 protected:
  EventFactoryEvdevSafetyChecksTest() {
    scoped_feature_list_.InitWithFeatureState(
        kExcludeEvdevDispatchFromSafetyChecks, GetParam());
    event_factory_ = std::make_unique<EventFactoryEvdev>(
        nullptr, &device_manager_, &keyboard_layout_engine_);
    event_factory_->SetUserInputTaskRunner(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    event_factory_->Init();
    event_factory_->AddPlatformEventObserver(&event_observer_);
    GamepadProviderOzone::GetInstance()->AddGamepadObserver(&gamepad_observer_);
  }

  ~EventFactoryEvdevSafetyChecksTest() override {
    GamepadProviderOzone::GetInstance()->RemoveGamepadObserver(
        &gamepad_observer_);
    event_factory_->RemovePlatformEventObserver(&event_observer_);
  }

  void SetUp() override {
    if (!DeviceDataManager::HasInstance()) {
      DeviceDataManager::CreateInstance();
      created_device_data_manager_ = true;
    }
    // Ensure the thread-local `ThreadCache` is initialized before reading
    // `PausedCount()`.
    std::make_unique<int>(0).reset();
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED) &&           \
    !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
    const int paused_before = GetQuarantinePausedCount();
    {
      base::ScopedSafetyChecksExclusion test_exclusion;
      ASSERT_EQ(GetQuarantinePausedCount(), paused_before + 1);
    }
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED) &&
        // !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
  }

  void TearDown() override {
    if (created_device_data_manager_) {
      DeviceDataManager::DeleteInstance();
    }
  }

  static int GetQuarantinePausedCount() {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED) &&           \
    !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
    return partition_alloc::internal::
        ScopedSchedulerLoopQuarantineBranchAccessorForTesting(
               allocator_shim::internal::PartitionAllocMalloc::Allocator())
            .PausedCount();
#else
    return 0;
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED) &&
        // !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
  }

  void ExpectUiDispatchPausedDelta(base::OnceClosure dispatch_call) {
    const int baseline = GetQuarantinePausedCount();
    [[maybe_unused]] const int expected_delta = GetParam() ? 1 : 0;
    bool dispatched = false;
    EXPECT_CALL(event_observer_, WillProcessEvent)
        .WillOnce([&](const PlatformEvent&) {
          dispatched = true;
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED) &&           \
    !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
          EXPECT_EQ(GetQuarantinePausedCount(), baseline + expected_delta);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED) &&
        // !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
        });
    std::move(dispatch_call).Run();
    EXPECT_TRUE(dispatched);
    EXPECT_EQ(GetQuarantinePausedCount(), baseline);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  StubKeyboardLayoutEngine keyboard_layout_engine_;
  testing::StrictMock<MockDeviceManager> device_manager_;
  testing::NiceMock<MockPlatformEventObserver> event_observer_;
  testing::StrictMock<MockGamepadObserver> gamepad_observer_;
  std::unique_ptr<EventFactoryEvdev> event_factory_;
  bool created_device_data_manager_ = false;
};

TEST_P(EventFactoryEvdevSafetyChecksTest, ExcludesAllInputEventDispatches) {
  ExpectUiDispatchPausedDelta(base::BindOnce(
      &EventFactoryEvdev::DispatchKeyEvent,
      base::Unretained(event_factory_.get()),
      KeyEventParams{0, EF_NONE, KEY_A, 0, true, false, EventTimeForNow()}));

  ExpectUiDispatchPausedDelta(base::BindOnce(
      &EventFactoryEvdev::DispatchMouseMoveEvent,
      base::Unretained(event_factory_.get()),
      MouseMoveEventParams{0, EF_NONE, gfx::PointF(10, 20), nullptr,
                           PointerDetails(EventPointerType::kMouse),
                           EventTimeForNow()}));

  ExpectUiDispatchPausedDelta(base::BindOnce(
      &EventFactoryEvdev::DispatchMouseButtonEvent,
      base::Unretained(event_factory_.get()),
      MouseButtonEventParams{0, EF_NONE, gfx::PointF(10, 20), BTN_LEFT, true,
                             MouseButtonMapType::kMouse,
                             PointerDetails(EventPointerType::kMouse),
                             EventTimeForNow()}));

  ExpectUiDispatchPausedDelta(base::BindOnce(
      &EventFactoryEvdev::DispatchMouseWheelEvent,
      base::Unretained(event_factory_.get()),
      MouseWheelEventParams{0, gfx::PointF(10, 20), gfx::Vector2d(0, 120),
                            gfx::Vector2d(0, 120), EventTimeForNow()}));

  ExpectUiDispatchPausedDelta(base::BindOnce(
      &EventFactoryEvdev::DispatchPinchEvent,
      base::Unretained(event_factory_.get()),
      PinchEventParams{0, EventType::kGesturePinchUpdate, gfx::PointF(10, 20),
                       1.2f, EventTimeForNow()}));

  ExpectUiDispatchPausedDelta(base::BindOnce(
      &EventFactoryEvdev::DispatchScrollEvent,
      base::Unretained(event_factory_.get()),
      ScrollEventParams{0, EventType::kScroll, gfx::PointF(10, 20),
                        gfx::Vector2dF(5, 5), gfx::Vector2dF(5, 5), 2,
                        EventTimeForNow()}));

  ExpectUiDispatchPausedDelta(base::BindOnce(
      &EventFactoryEvdev::DispatchTouchEvent,
      base::Unretained(event_factory_.get()),
      TouchEventParams{0, 0, EventType::kTouchPressed, gfx::PointF(10, 20),
                       PointerDetails(EventPointerType::kTouch),
                       EventTimeForNow(), EF_NONE}));

  const int baseline = GetQuarantinePausedCount();
  [[maybe_unused]] const int expected_delta = GetParam() ? 1 : 0;
  bool gamepad_dispatched = false;
  EXPECT_CALL(gamepad_observer_, OnGamepadEvent)
      .WillOnce([&](const GamepadEvent&) {
        gamepad_dispatched = true;
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED) &&           \
    !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
        EXPECT_EQ(GetQuarantinePausedCount(), baseline + expected_delta);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // PA_CONFIG(THREAD_CACHE_SUPPORTED) &&
        // !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
      });
  event_factory_->DispatchGamepadEvent(GamepadEvent(
      0, GamepadEventType::BUTTON, BTN_SOUTH, 1.0, EventTimeForNow()));
  EXPECT_TRUE(gamepad_dispatched);
  EXPECT_EQ(GetQuarantinePausedCount(), baseline);
}

INSTANTIATE_TEST_SUITE_P(All,
                         EventFactoryEvdevSafetyChecksTest,
                         testing::Bool());

}  // namespace
}  // namespace ui
