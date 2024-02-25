// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/touchscreen_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/input_device.h"

namespace ui {

namespace {

constexpr char kTestDescription[] =
    R"(class=ui::TouchscreenDevice id=123
 size=1001x3008
 touch_points=4
 has_stylus=0
 has_stylus_garage_switch=1
base class=ui::InputDevice id=123
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

TEST(TouchscreenDeviceTest, Description) {
  ui::TouchscreenDevice device(
      ui::InputDevice(
          123, ui::InputDeviceType::INPUT_DEVICE_USB, "Name", "Phys",
          base::FilePath(FILE_PATH_LITERAL("/sys/some/path/event8")), 0x0012,
          0xBE00, 0x18AF),
      gfx::Size(1001, 3008), 4, false, true);

  std::stringstream os;

  device.DescribeForLog(os);

  EXPECT_EQ(os.str(), kTestDescription);
}

}  // namespace ui
