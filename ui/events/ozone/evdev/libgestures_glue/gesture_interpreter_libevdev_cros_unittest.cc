// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/gesture_interpreter_libevdev_cros.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/events/ozone/chromeos/cursor_controller.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/libgestures_glue/gesture_property_provider.h"
#include "ui/events/ozone/evdev/testing/fake_cursor_delegate_evdev.h"

namespace ui {

namespace {

class TestDeviceEventDispatcherEvdev : public DeviceEventDispatcherEvdev {
 public:
  const std::optional<ScrollEventParams>& last_scroll() const {
    return last_scroll_;
  }

  // DeviceEventDispatcherEvdev:
  void DispatchKeyEvent(const KeyEventParams& params) override {}
  void DispatchMouseMoveEvent(const MouseMoveEventParams& params) override {}
  void DispatchMouseButtonEvent(const MouseButtonEventParams& params) override {
  }
  void DispatchMouseWheelEvent(const MouseWheelEventParams& params) override {}
  void DispatchPinchEvent(const PinchEventParams& params) override {}
  void DispatchScrollEvent(const ScrollEventParams& params) override {
    last_scroll_.emplace(params);
  }
  void DispatchTouchEvent(const TouchEventParams& params) override {}
  void DispatchGamepadEvent(const GamepadEvent& event) override {}
  void DispatchMicrophoneMuteSwitchValueChanged(bool muted) override {}
  void DispatchAnyKeysPressedUpdated(bool any) override {}
  void DispatchKeyboardDevicesUpdated(
      const std::vector<KeyboardDevice>& devices,
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  void DispatchTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) override {}
  void DispatchMouseDevicesUpdated(const std::vector<InputDevice>& devices,
                                   bool has_mouse) override {}
  void DispatchPointingStickDevicesUpdated(
      const std::vector<InputDevice>& devices) override {}
  void DispatchTouchpadDevicesUpdated(
      const std::vector<TouchpadDevice>& devices,
      bool has_haptic_touchpad) override {}
  void DispatchGraphicsTabletDevicesUpdated(
      const std::vector<InputDevice>& devices) override {}
  void DispatchDeviceListsComplete() override {}
  void DispatchStylusStateChanged(StylusState stylus_state) override {}
  void DispatchGamepadDevicesUpdated(
      const std::vector<GamepadDevice>& devices,
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  void DispatchUncategorizedDevicesUpdated(
      const std::vector<InputDevice>& devices) override {}

 private:
  std::optional<ScrollEventParams> last_scroll_;
};

}  // namespace

TEST(GestureInterpreterLibevdevCrosTest, RotatesInternalDeviceOnly) {
  CursorController::GetInstance()->SetInternalDisplayRotation(
      display::Display::ROTATE_90);

  FakeCursorDelegateEvdev cursor;
  GesturePropertyProvider property_provider;
  TestDeviceEventDispatcherEvdev dispatcher;

  Gesture move(kGestureMove, /*start=*/0, /*end=*/0, /*dx=*/3.f, /*dy=*/5.f);
  Gesture scroll(kGestureScroll, /*start=*/0, /*end=*/0, /*dx=*/3.f,
                 /*dy=*/5.f);

  GestureInterpreterLibevdevCros internal_device(
      /*id=*/1, &cursor, &property_provider, &dispatcher,
      /*is_internal_device=*/true);
  internal_device.OnGestureReady(&move);
  EXPECT_FLOAT_EQ(5.f, cursor.GetLocation().x());
  EXPECT_FLOAT_EQ(-3.f, cursor.GetLocation().y());
  internal_device.OnGestureReady(&scroll);
  ASSERT_TRUE(dispatcher.last_scroll().has_value());
  EXPECT_FLOAT_EQ(5.f, dispatcher.last_scroll()->delta.x());
  EXPECT_FLOAT_EQ(-3.f, dispatcher.last_scroll()->delta.y());

  GestureInterpreterLibevdevCros external_device(
      /*id=*/2, &cursor, &property_provider, &dispatcher,
      /*is_internal_device=*/false);
  external_device.OnGestureReady(&move);
  EXPECT_FLOAT_EQ(3.f, cursor.GetLocation().x());
  EXPECT_FLOAT_EQ(5.f, cursor.GetLocation().y());

  CursorController::GetInstance()->SetInternalDisplayRotation(
      display::Display::ROTATE_0);
}

}  // namespace ui
