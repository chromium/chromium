// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_device_info.h"

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "event_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/event_device_util.h"

namespace ui {

TEST(EventDeviceInfoTest, BasicUsbGamepad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kXboxGamepad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_TRUE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicCrosKeyboard) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLinkKeyboard, &devinfo));

  EXPECT_TRUE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_TRUE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, SideVolumeButton) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kSideVolumeButton, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());
}

TEST(EventDeviceInfoTest, BasicCrosTouchscreen) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLinkTouchscreen, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicCrosTouchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLinkTouchpad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, HapticCrosTouchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kRedrixTouchpad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_TRUE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicCrosPointingStick) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kMorphiusPointingStick, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_TRUE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbKeyboard) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHpUsbKeyboard, &devinfo));

  EXPECT_TRUE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_TRUE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbKeyboard_Extra) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHpUsbKeyboard_Extra, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());  // Has keys, but not a full keyboard.
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbMouse) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLogitechUsbMouse, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_TRUE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
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
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbTablet) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kWacomIntuosPtS_Pen, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_TRUE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicUsbTouchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kWacomIntuosPtS_Finger, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
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
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
}

TEST(EventDeviceInfoTest, AbsoluteMouseTouchscreen) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kElo_TouchSystems_2700, &devinfo));

  // This touchscreen uses BTN_LEFT for touch contact.
  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, OnScreenStylus) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kWilsonBeachActiveStylus, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, HammerKeyboard) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHammerKeyboard, &devinfo));

  EXPECT_TRUE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, HammerTouchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHammerTouchpad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, IllitekTP_Mouse) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kIlitekTP_Mouse, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, IllitekTP) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kIlitekTP, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_USB, devinfo.device_type());
}

TEST(EventDeviceInfoTest, Nocturne_Touchscreen) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kNocturneTouchScreen, &devinfo));
  EXPECT_EQ(40, devinfo.GetAbsResolution(ABS_MT_POSITION_X));
  EXPECT_EQ(10404, devinfo.GetAbsMaximum(ABS_MT_POSITION_X));
}

TEST(EventDeviceInfoTest, XboxElite) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kXboxElite, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_TRUE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH, devinfo.device_type());
}

TEST(EventDeviceInfoTest, DellActivePen_Button) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kDellActivePenButton, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_TRUE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicStylusGarageSwitch) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kDrawciaStylusGarage, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_TRUE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_UNKNOWN, devinfo.device_type());
}

TEST(EventDeviceInfoTest, BasicDynamicNumberpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kDrobitNumberpad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_TRUE(devinfo.HasNumberpad());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, DellLatitudeE6510Touchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kDellLatitudeE6510Touchpad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasValidMTAbsXY());
  EXPECT_TRUE(devinfo.IsSemiMultitouch());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, HPProBook6560bTouchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHPProBook6560bTouchpad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_TRUE(devinfo.HasValidMTAbsXY());
  EXPECT_TRUE(devinfo.IsSemiMultitouch());

  EXPECT_EQ(ui::InputDeviceType::INPUT_DEVICE_INTERNAL, devinfo.device_type());
}

TEST(EventDeviceInfoTest, DeviceOnKeyboardBlocklist) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kSymbolTechBarcodeScanner, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_FALSE(devinfo.HasValidMTAbsXY());
  EXPECT_FALSE(devinfo.IsSemiMultitouch());
}

TEST(EventDeviceInfoTest, EventDeviceTypeDescriptions) {
  auto fmt = [](auto value) {
    std::stringstream s;
    s << value;
    return s.str();
  };
  EXPECT_EQ("ui::EventDeviceType::DT_KEYBOARD",
            fmt(ui::EventDeviceType::DT_KEYBOARD));
  EXPECT_EQ("ui::EventDeviceType::DT_MOUSE",
            fmt(ui::EventDeviceType::DT_MOUSE));
  EXPECT_EQ("ui::EventDeviceType::DT_POINTING_STICK",
            fmt(ui::EventDeviceType::DT_POINTING_STICK));
  EXPECT_EQ("ui::EventDeviceType::DT_TOUCHPAD",
            fmt(ui::EventDeviceType::DT_TOUCHPAD));
  EXPECT_EQ("ui::EventDeviceType::DT_TOUCHSCREEN",
            fmt(ui::EventDeviceType::DT_TOUCHSCREEN));
  EXPECT_EQ("ui::EventDeviceType::DT_MULTITOUCH",
            fmt(ui::EventDeviceType::DT_MULTITOUCH));
  EXPECT_EQ("ui::EventDeviceType::DT_MULTITOUCH_MOUSE",
            fmt(ui::EventDeviceType::DT_MULTITOUCH_MOUSE));
  EXPECT_EQ("ui::EventDeviceType::DT_ALL", fmt(ui::EventDeviceType::DT_ALL));
  EXPECT_EQ("ui::EventDeviceType::unknown_value(1234)",
            fmt(static_cast<ui::EventDeviceType>(1234)));
}

TEST(EventDeviceInfoTest, KeyboardTypeDescriptions) {
  auto fmt = [](auto value) {
    std::stringstream s;
    s << value;
    return s.str();
  };
  EXPECT_EQ("ui::KeyboardType::NOT_KEYBOARD",
            fmt(ui::KeyboardType::NOT_KEYBOARD));
  EXPECT_EQ("ui::KeyboardType::IN_BLOCKLIST",
            fmt(ui::KeyboardType::IN_BLOCKLIST));
  EXPECT_EQ("ui::KeyboardType::STYLUS_BUTTON_DEVICE",
            fmt(ui::KeyboardType::STYLUS_BUTTON_DEVICE));
  EXPECT_EQ("ui::KeyboardType::VALID_KEYBOARD",
            fmt(ui::KeyboardType::VALID_KEYBOARD));
  EXPECT_EQ("ui::KeyboardType::unknown_value(2345)",
            fmt(static_cast<ui::KeyboardType>(2345)));
}

TEST(EventDeviceInfoTest, RexHeatmapTouchScreen) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kRexHeatmapTouchScreen, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasPointingStick());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasHapticTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
  EXPECT_FALSE(devinfo.HasGamepad());
  EXPECT_FALSE(devinfo.IsStylusButtonDevice());
  EXPECT_FALSE(devinfo.HasStylusSwitch());
  EXPECT_TRUE(devinfo.SupportsHeatmap());
}

}  // namespace ui
