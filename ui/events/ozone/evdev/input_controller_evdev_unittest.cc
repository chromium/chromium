// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_controller_evdev.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(InputControllerEvdevTest, AccelerationSuspension) {
  InputControllerEvdev controller(nullptr, nullptr, nullptr);
  controller.SetMouseAcceleration(true);
  controller.SetPointingStickAcceleration(true);

  EXPECT_TRUE(controller.input_device_settings_.mouse_acceleration_enabled);
  EXPECT_TRUE(
      controller.input_device_settings_.pointing_stick_acceleration_enabled);

  // Suspending should disable the acceleration temporarily.
  controller.SuspendMouseAcceleration();
  EXPECT_FALSE(controller.input_device_settings_.mouse_acceleration_enabled);
  EXPECT_FALSE(
      controller.input_device_settings_.pointing_stick_acceleration_enabled);

  // Resuming should enable it again.
  controller.EndMouseAccelerationSuspension();
  EXPECT_TRUE(controller.input_device_settings_.mouse_acceleration_enabled);
  EXPECT_TRUE(
      controller.input_device_settings_.pointing_stick_acceleration_enabled);
}

TEST(InputControllerEvdevTest, AccelerationChangeDuringSuspension) {
  InputControllerEvdev controller(nullptr, nullptr, nullptr);
  controller.SetMouseAcceleration(true);
  controller.SetPointingStickAcceleration(true);

  // Suspending should disable the acceleration temporarily.
  controller.SuspendMouseAcceleration();
  EXPECT_FALSE(controller.input_device_settings_.mouse_acceleration_enabled);
  EXPECT_FALSE(
      controller.input_device_settings_.pointing_stick_acceleration_enabled);

  // Settings changes while suspended should not take effect immediately...
  controller.SetMouseAcceleration(true);
  controller.SetPointingStickAcceleration(true);
  EXPECT_FALSE(controller.input_device_settings_.mouse_acceleration_enabled);
  EXPECT_FALSE(
      controller.input_device_settings_.pointing_stick_acceleration_enabled);

  // ...instead being applied when the suspension ends.
  controller.SetMouseAcceleration(false);
  controller.SetPointingStickAcceleration(false);
  controller.EndMouseAccelerationSuspension();
  EXPECT_FALSE(controller.input_device_settings_.mouse_acceleration_enabled);
  EXPECT_FALSE(
      controller.input_device_settings_.pointing_stick_acceleration_enabled);
}

}  // namespace ui
