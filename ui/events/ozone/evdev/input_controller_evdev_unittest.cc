// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_controller_evdev.h"
#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/input_device_settings_evdev.h"

namespace ui {

TEST(InputControllerEvdevTest, AccelerationSuspension) {
  InputControllerEvdev controller(nullptr, nullptr, nullptr);
  controller.SetMouseAcceleration(std::nullopt, true);
  controller.SetPointingStickAcceleration(std::nullopt, true);

  EXPECT_TRUE(controller.GetInputDeviceSettings()
                  .GetMouseSettings()
                  .acceleration_enabled);
  EXPECT_TRUE(controller.GetInputDeviceSettings()
                  .GetPointingStickSettings()
                  .acceleration_enabled);
  EXPECT_FALSE(controller.GetInputDeviceSettings().suspend_acceleration);

  // Suspending should disable the acceleration temporarily.
  controller.SuspendMouseAcceleration();
  EXPECT_TRUE(controller.GetInputDeviceSettings()
                  .GetMouseSettings()
                  .acceleration_enabled);
  EXPECT_TRUE(controller.GetInputDeviceSettings()
                  .GetPointingStickSettings()
                  .acceleration_enabled);
  EXPECT_TRUE(controller.GetInputDeviceSettings().suspend_acceleration);

  // Resuming should enable it again.
  controller.EndMouseAccelerationSuspension();
  EXPECT_TRUE(controller.GetInputDeviceSettings()
                  .GetMouseSettings()
                  .acceleration_enabled);
  EXPECT_TRUE(controller.GetInputDeviceSettings()
                  .GetPointingStickSettings()
                  .acceleration_enabled);
  EXPECT_FALSE(controller.GetInputDeviceSettings().suspend_acceleration);
}

TEST(InputControllerEvdevTest, DisableInputDevices) {
  InputControllerEvdev controller(nullptr, nullptr, nullptr);

  EXPECT_TRUE(controller.AreInputDevicesEnabled());

  // Calling `DisableInputDevices()` should disable the input devices.
  auto first_scoped_disable_input_devices = controller.DisableInputDevices();
  EXPECT_FALSE(controller.AreInputDevicesEnabled());
  EXPECT_FALSE(controller.GetInputDeviceSettings().enable_devices);

  // Input devices should remain disabled after multiple calls to
  // `DisableInputDevices()`.
  auto second_scoped_disable_input_devices = controller.DisableInputDevices();
  auto third_scoped_disable_input_devices = controller.DisableInputDevices();
  EXPECT_FALSE(controller.AreInputDevicesEnabled());

  // And they should only be enabled after the last scoped blocker is gone.
  first_scoped_disable_input_devices.reset();
  EXPECT_FALSE(controller.AreInputDevicesEnabled());
  third_scoped_disable_input_devices.reset();
  EXPECT_FALSE(controller.AreInputDevicesEnabled());
  second_scoped_disable_input_devices.reset();
  EXPECT_TRUE(controller.AreInputDevicesEnabled());
}

TEST(InputControllerEvdevTest, ScopedDisableInputDevicesCanOutliveController) {
  std::unique_ptr<ScopedDisableInputDevices> scoped_disabler;

  {
    InputControllerEvdev controller(nullptr, nullptr, nullptr);
    scoped_disabler = controller.DisableInputDevices();
  }

  // deleting the disabler after the controller is gone should not lead to
  // use-after-free bugs.
  scoped_disabler.reset();
}

}  // namespace ui
