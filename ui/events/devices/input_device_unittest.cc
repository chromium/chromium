// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/input_device.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/input_device.h"

namespace ui {

namespace {

constexpr char kTestDescription[] = R"(class=ui::InputDevice id=12
 input_device_type=ui::InputDeviceType::INPUT_DEVICE_USB
 name="Name"
 phys="Phys"
 enabled=1
 suspected_keyboard_imposter=0
 suspected_mouse_imposter=0
 sys_path="/sys/some/path/event8"
 vendor_id=0012
 product_id=BE00
 version=18AF
)";

}  // namespace

TEST(InputDeviceTest, TestDescription) {
  ui::InputDevice device(
      12, ui::InputDeviceType::INPUT_DEVICE_USB, "Name", "Phys",
      base::FilePath(FILE_PATH_LITERAL("/sys/some/path/event8")), 0x0012,
      0xBE00, 0x18AF);

  std::stringstream os;

  device.DescribeForLog(os);

  EXPECT_EQ(os.str(), kTestDescription);
}

TEST(InputDeviceTest, InputDeviceTypeDescriptions) {
  auto fmt = [](auto value) {
    std::stringstream s;
    s << value;
    return s.str();
  };
  EXPECT_EQ("ui::InputDeviceType::INPUT_DEVICE_INTERNAL",
            fmt(ui::InputDeviceType::INPUT_DEVICE_INTERNAL));
  EXPECT_EQ("ui::InputDeviceType::INPUT_DEVICE_USB",
            fmt(ui::InputDeviceType::INPUT_DEVICE_USB));
  EXPECT_EQ("ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH",
            fmt(ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH));
  EXPECT_EQ("ui::InputDeviceType::INPUT_DEVICE_UNKNOWN",
            fmt(ui::InputDeviceType::INPUT_DEVICE_UNKNOWN));
  EXPECT_EQ("ui::InputDeviceType::unknown_value(123)",
            fmt(static_cast<ui::InputDeviceType>(123)));
}

}  // namespace ui
