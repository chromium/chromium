// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_controller_evdev.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/input_device_settings_evdev.h"

namespace ui {

TEST(InputControllerEvdevTest, AccelerationSuspension) {
  InputControllerEvdev controller(nullptr, nullptr, nullptr);
  controller.SetMouseAcceleration(absl::nullopt, true);
  controller.SetPointingStickAcceleration(absl::nullopt, true);

  EXPECT_TRUE(controller.input_device_settings_.GetMouseSettings()
                  .acceleration_enabled);
  EXPECT_TRUE(controller.input_device_settings_.GetPointingStickSettings()
                  .acceleration_enabled);
  EXPECT_FALSE(controller.input_device_settings_.suspend_acceleration);

  // Suspending should disable the acceleration temporarily.
  controller.SuspendMouseAcceleration();
  EXPECT_TRUE(controller.input_device_settings_.GetMouseSettings()
                  .acceleration_enabled);
  EXPECT_TRUE(controller.input_device_settings_.GetPointingStickSettings()
                  .acceleration_enabled);
  EXPECT_TRUE(controller.input_device_settings_.suspend_acceleration);

  // Resuming should enable it again.
  controller.EndMouseAccelerationSuspension();
  EXPECT_TRUE(controller.input_device_settings_.GetMouseSettings()
                  .acceleration_enabled);
  EXPECT_TRUE(controller.input_device_settings_.GetPointingStickSettings()
                  .acceleration_enabled);
  EXPECT_FALSE(controller.input_device_settings_.suspend_acceleration);
}

}  // namespace ui
