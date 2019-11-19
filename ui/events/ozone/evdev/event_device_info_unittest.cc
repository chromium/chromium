// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/event_device_util.h"

namespace ui {

TEST(EventDeviceInfoTest, BasicUsbGamepad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kXboxGamepad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_TRUE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicCrosKeyboard) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLinkKeyboard, &devinfo));

  EXPECT_TRUE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, SideVolumeButton) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kSideVolumeButton, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
}

TEST(EventDeviceInfoTest, BasicCrosTouchscreen) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLinkTouchscreen, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicCrosTouchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLinkTouchpad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbKeyboard) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHpUsbKeyboard, &devinfo));

  EXPECT_TRUE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbKeyboard_Extra) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHpUsbKeyboard_Extra, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());  // Has keys, but not a full keyboard.
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbMouse) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLogitechUsbMouse, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_TRUE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbTouchscreen) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kMimoTouch2Touchscreen, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbTablet) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kWacomIntuosPtS_Pen, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_TRUE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbTouchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kWacomIntuosPtS_Finger, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, HybridKeyboardWithMouse) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLogitechTouchKeyboardK400, &devinfo));

  // The touchpad actually exposes mouse (relative) Events.
  EXPECT_TRUE(devinfo.HasKeyboard());
  EXPECT_TRUE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
}

TEST(EventDeviceInfoTest, AbsoluteMouseTouchscreen) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kElo_TouchSystems_2700, &devinfo));

  // This touchscreen uses BTN_LEFT for touch contact.
  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, OnScreenStylus) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kWilsonBeachActiveStylus, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, HammerKeyboard) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHammerKeyboard, &devinfo));

  EXPECT_TRUE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, HammerTouchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHammerTouchpad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, IllitekTP_Mouse) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kIlitekTP_Mouse, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, IllitekTP) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kIlitekTP, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}
TEST(EventDeviceInfoTest, Nocturne_Touchscreen) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kNocturneTouchScreen, &devinfo));
  EXPECT_EQ(40, devinfo.GetAbsResolution(ABS_MT_POSITION_X));
  EXPECT_EQ(10404, devinfo.GetAbsMaximum(ABS_MT_POSITION_X));
}

}  // namespace ui
