// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/event_device_test_util.h"

#include <stdint.h>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_util.h"

namespace ui {

namespace {

// This test requres 64 bit groups in bitmask inputs (merge them if 32-bit).
const int kTestDataWordSize = 64;

#define EVDEV_BITS_TO_GROUPS(x) \
  (((x) + kTestDataWordSize - 1) / kTestDataWordSize)

std::string SerializeBitfield(unsigned long* bitmap, int max) {
  std::string ret;

  for (int i = EVDEV_BITS_TO_GROUPS(max) - 1; i >= 0; i--) {
    if (bitmap[i] || ret.size()) {
      base::StringAppendF(&ret, "%lx", bitmap[i]);

      if (i > 0)
        ret += " ";
    }
  }

  if (ret.length() == 0)
    ret = "0";

  return ret;
}

bool ParseBitfield(const std::string& bitfield,
                   size_t max_bits,
                   std::vector<unsigned long>* out) {
  std::vector<std::string> groups = base::SplitString(
      bitfield, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  out->resize(EVDEV_BITS_TO_LONGS(max_bits));

  // Convert big endian 64-bit groups to little endian EVDEV_LONG_BIT groups.
  for (size_t i = 0; i < groups.size(); ++i) {
    int off = groups.size() - 1 - i;

    uint64_t val;
    if (!base::HexStringToUInt64(groups[off], &val))
      return false;

    for (int j = 0; j < kTestDataWordSize; ++j) {
      unsigned int code = i * kTestDataWordSize + j;

      if (code >= max_bits)
        break;

      if (val & (1UL << j))
        EvdevSetBit(&(*out)[0], code);
    }
  }

  // Require canonically formatted input.
  if (bitfield != SerializeBitfield(out->data(), max_bits))
    return false;

  return true;
}

}  // namespace

// Captured from HJC Game ZD - V gamepad.
const DeviceAbsoluteAxis kHJCGamepadAbsAxes[] = {
    {ABS_X, {0, 0, 255, 0, 15, 0}},   {ABS_Y, {0, 0, 255, 0, 15, 0}},
    {ABS_Z, {0, 0, 255, 0, 15, 0}},   {ABS_RZ, {0, 0, 255, 0, 15, 0}},
    {ABS_HAT0X, {0, -1, 1, 0, 0, 0}}, {ABS_HAT0Y, {0, -1, 1, 0, 0, 0}},
};
const DeviceCapabilities kHJCGamepad = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-1/1-1:1.0/"
    "0003:11C5:5506.0005/input/input11/event8",
    /* name */ "HJC Game ZD - V",
    /* phys */ "usb-0000:00:14.0-1/input0",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "11c5",
    /* product */ "5506",
    /* version */ "0111",
    /* prop */ "0",
    /* ev */ "1b",
    /* key */ "fff000000000000 0 0 0 0",
    /* rel */ "0",
    /* abs */ "30027",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kHJCGamepadAbsAxes,
    arraysize(kHJCGamepadAbsAxes),
};

// Captured from Xbox 360 gamepad.
const DeviceAbsoluteAxis kXboxGamepadAbsAxes[] = {
    {ABS_X, {0, -32768, 32767, 16, 128, 0}},
    {ABS_Y, {0, -32768, 32767, 16, 128, 0}},
    {ABS_Z, {0, 0, 255, 0, 0, 0}},
    {ABS_RX, {0, -32768, 32767, 16, 128, 0}},
    {ABS_RY, {0, -32768, 32767, 16, 128, 0}},
    {ABS_RZ, {0, 0, 255, 0, 0, 0}},
    {ABS_HAT0X, {0, -1, 1, 0, 0, 0}},
    {ABS_HAT0Y, {0, -1, 1, 0, 0, 0}},
};
const DeviceCapabilities kXboxGamepad = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-1/1-1:1.0/input/input9/event8",
    /* name */ "Microsoft X-Box 360 pad",
    /* phys */ "usb-0000:00:14.0-1/input0",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "045e",
    /* product */ "028e",
    /* version */ "0114",
    /* prop */ "0",
    /* ev */ "20000b",
    /* key */ "7cdb000000000000 0 0 0 0",
    /* rel */ "0",
    /* abs */ "3003f",
    /* msc */ "0",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "107030000 0",
    kXboxGamepadAbsAxes,
    arraysize(kXboxGamepadAbsAxes),
};

// Captured from iBuffalo gamepad.
const DeviceAbsoluteAxis kiBuffaloGamepadAbsAxes[] = {
    {ABS_X, {0, 0, 255, 0, 15, 0}},
    {ABS_Y, {0, 0, 255, 0, 15, 0}},
};
const DeviceCapabilities kiBuffaloGamepad = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-1/"
    "1-1:1.0/0003:0583:2060.0004/input/input10/event8",
    /* name */ "USB,2-axis 8-button gamepad  ",
    /* phys */ "usb-0000:00:14.0-1/input0",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "0583",
    /* product */ "2060",
    /* version */ "0110",
    /* prop */ "0",
    /* ev */ "1b",
    /* key */ "ff00000000 0 0 0 0",
    /* rel */ "0",
    /* abs */ "3",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kiBuffaloGamepadAbsAxes,
    arraysize(kiBuffaloGamepadAbsAxes),
};

// Captured from Pixelbook.
const DeviceAbsoluteAxis kEveTouchScreenAbsAxes[] = {
    {ABS_X, {0, 0, 10368, 0, 0, 40}},
    {ABS_Y, {0, 0, 6912, 0, 0, 40}},
    {ABS_PRESSURE, {0, 0, 255, 0, 0, 0}},
    {ABS_MT_SLOT, {0, 0, 9, 0, 0, 0}},
    {ABS_MT_TOUCH_MAJOR, {0, 0, 255, 0, 0, 1}},
    {ABS_MT_TOUCH_MINOR, {0, 0, 255, 0, 0, 1}},
    {ABS_MT_ORIENTATION, {0, 0, 1, 0, 0, 0}},
    {ABS_MT_POSITION_X, {0, 0, 10368, 0, 0, 40}},
    {ABS_MT_POSITION_Y, {0, 0, 6912, 0, 0, 40}},
    {ABS_MT_TOOL_TYPE, {0, 0, 2, 0, 0, 0}},
    {ABS_MT_TRACKING_ID, {0, 0, 65535, 0, 0, 0}},
    {ABS_MT_PRESSURE, {0, 0, 255, 0, 0, 0}},
};
const DeviceCapabilities kEveTouchScreen = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:15.0/i2c_designware.0/i2c-6/"
    "i2c-WCOM50C1:00/0018:2D1F:5143.0001/input/input4/event4",
    /* name */ "WCOM50C1:00 2D1F:5143",
    /* phys */ "i2c-WCOM50C1:00",
    /* uniq */ "",
    /* bustype */ "0018",
    /* vendor */ "2d1f",
    /* product */ "5143",
    /* version */ "0100",
    /* prop */ "2",
    /* ev */ "1b",
    /* key */ "400 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "6f3800001000003",
    /* msc */ "20",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kEveTouchScreenAbsAxes,
    arraysize(kEveTouchScreenAbsAxes),
};

// Captured from Chromebook Pixel.
const DeviceCapabilities kLinkKeyboard = {
    /* path */ "/sys/devices/platform/i8042/serio0/input/input6/event6",
    /* name */ "AT Translated Set 2 keyboard",
    /* phys */ "isa0060/serio0/input0",
    /* uniq */ "",
    /* bustype */ "0011",
    /* vendor */ "0001",
    /* product */ "0001",
    /* version */ "ab83",
    /* prop */ "0",
    /* ev */ "120013",
    /* key */ "400402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe",
    /* rel */ "0",
    /* abs */ "0",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "7",
    /* ff */ "0",
};

// Captured from Chromebook Pixel.
const DeviceAbsoluteAxis kLinkTouchscreenAbsAxes[] = {
    {ABS_X, {0, 0, 2559, 0, 0, 20}},
    {ABS_Y, {0, 0, 1699, 0, 0, 20}},
    {ABS_PRESSURE, {0, 0, 255, 0, 0, 0}},
    {ABS_MT_SLOT, {0, 0, 15, 0, 0, 0}},
    {ABS_MT_TOUCH_MAJOR, {0, 0, 938, 0, 0, 0}},
    {ABS_MT_ORIENTATION, {0, -3, 4, 0, 0, 0}},
    {ABS_MT_POSITION_X, {0, 0, 2559, 0, 0, 20}},
    {ABS_MT_POSITION_Y, {0, 0, 1699, 0, 0, 20}},
    {ABS_MT_TRACKING_ID, {0, 0, 65535, 0, 0, 0}},
    {ABS_MT_PRESSURE, {0, 0, 255, 0, 0, 0}},
};
const DeviceCapabilities kLinkTouchscreen = {
    /* path */ "/sys/devices/pci0000:00/0000:00:02.0/i2c-2/2-004a/"
               "input/input7/event7",
    /* name */ "Atmel maXTouch Touchscreen",
    /* phys */ "i2c-2-004a/input0",
    /* uniq */ "",
    /* bustype */ "0018",
    /* vendor */ "0000",
    /* product */ "0000",
    /* version */ "0000",
    /* prop */ "0",
    /* ev */ "b",
    /* key */ "400 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "671800001000003",
    /* msc */ "0",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kLinkTouchscreenAbsAxes,
    arraysize(kLinkTouchscreenAbsAxes),
};

// Fake Atmel touchscreen based on real device from Chromebook Pixel,
// with the addition of ABS_MT_TOOL_TYPE capability.
const DeviceAbsoluteAxis kLinkWithToolTypeTouchscreenAbsAxes[] = {
    {ABS_X, {0, 0, 2559, 0, 0, 20}},
    {ABS_Y, {0, 0, 1699, 0, 0, 20}},
    {ABS_PRESSURE, {0, 0, 255, 0, 0, 0}},
    {ABS_MT_SLOT, {0, 0, 15, 0, 0, 0}},
    {ABS_MT_TOUCH_MAJOR, {0, 0, 938, 0, 0, 0}},
    {ABS_MT_ORIENTATION, {0, -3, 4, 0, 0, 0}},
    {ABS_MT_POSITION_X, {0, 0, 2559, 0, 0, 20}},
    {ABS_MT_POSITION_Y, {0, 0, 1699, 0, 0, 20}},
    {ABS_MT_TOOL_TYPE, {0, 0, 0, 0, 0, 0}},
    {ABS_MT_TRACKING_ID, {0, 0, 65535, 0, 0, 0}},
    {ABS_MT_PRESSURE, {0, 0, 255, 0, 0, 0}},
};
const DeviceCapabilities kLinkWithToolTypeTouchscreen = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:02.0/i2c-2/2-004a/"
    "input/input7/event7",
    /* name */ "Atmel maXTouch Touchscreen",
    /* phys */ "i2c-2-004a/input0",
    /* uniq */ "",
    /* bustype */ "0018",
    /* vendor */ "0000",
    /* product */ "0000",
    /* version */ "0000",
    /* prop */ "0",
    /* ev */ "b",
    /* key */ "400 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "673800001000003",
    /* msc */ "0",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kLinkWithToolTypeTouchscreenAbsAxes,
    arraysize(kLinkWithToolTypeTouchscreenAbsAxes),
};

// Captured from Chromebook Pixel.
const DeviceAbsoluteAxis kLinkTouchpadAbsAxes[] = {
    {ABS_X, {0, 0, 2040, 0, 0, 20}},
    {ABS_Y, {0, 0, 1360, 0, 0, 20}},
    {ABS_PRESSURE, {0, 0, 255, 0, 0, 0}},
    {ABS_MT_SLOT, {0, 0, 9, 0, 0, 0}},
    {ABS_MT_TOUCH_MAJOR, {0, 0, 1878, 0, 0, 0}},
    {ABS_MT_ORIENTATION, {0, -3, 4, 0, 0, 0}},
    {ABS_MT_POSITION_X, {0, 0, 2040, 0, 0, 20}},
    {ABS_MT_POSITION_Y, {0, 0, 1360, 0, 0, 20}},
    {ABS_MT_TRACKING_ID, {0, 0, 65535, 0, 0, 0}},
    {ABS_MT_PRESSURE, {0, 0, 255, 0, 0, 0}},
};
const DeviceCapabilities kLinkTouchpad = {
    /* path */ "/sys/devices/pci0000:00/0000:00:02.0/i2c-1/1-004b/"
               "input/input8/event8",
    /* name */ "Atmel maXTouch Touchpad",
    /* phys */ "i2c-1-004b/input0",
    /* uniq */ "",
    /* bustype */ "0018",
    /* vendor */ "0000",
    /* product */ "0000",
    /* version */ "0000",
    /* prop */ "5",
    /* ev */ "b",
    /* key */ "e520 10000 0 0 0 0",
    /* rel */ "0",
    /* abs */ "671800001000003",
    /* msc */ "0",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kLinkTouchpadAbsAxes,
    arraysize(kLinkTouchpadAbsAxes),
};

// Captured from generic HP KU-1156 USB keyboard.
const DeviceCapabilities kHpUsbKeyboard = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.3/2-1.3:1.0/"
               "input/input17/event10",
    /* name */ "Chicony HP Elite USB Keyboard",
    /* phys */ "usb-0000:00:1d.0-1.3/input0",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "03f0",
    /* product */ "034a",
    /* version */ "0110",
    /* prop */ "0",
    /* ev */ "120013",
    /* key */ "1000000000007 ff9f207ac14057ff febeffdfffefffff "
              "fffffffffffffffe",
    /* rel */ "0",
    /* abs */ "0",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "7",
    /* ff */ "0",
};

// Captured from generic HP KU-1156 USB keyboard (2nd device with media keys).
const DeviceAbsoluteAxis kHpUsbKeyboard_ExtraAbsAxes[] = {
    {ABS_VOLUME, {0, 0, 767, 0, 0, 0}},
};
const DeviceCapabilities kHpUsbKeyboard_Extra = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.3/2-1.3:1.1/"
               "input/input18/event16",
    /* name */ "Chicony HP Elite USB Keyboard",
    /* phys */ "usb-0000:00:1d.0-1.3/input1",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "03f0",
    /* product */ "034a",
    /* version */ "0110",
    /* prop */ "0",
    /* ev */ "1f",
    /* key */ "3007f 0 0 483ffff17aff32d bf54444600000000 1 120f938b17c000 "
              "677bfad941dfed 9ed68000004400 10000002",
    /* rel */ "40",
    /* abs */ "100000000",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kHpUsbKeyboard_ExtraAbsAxes,
    arraysize(kHpUsbKeyboard_ExtraAbsAxes),
};

// Captured from Dell MS111-L 3-Button Optical USB Mouse.
const DeviceCapabilities kLogitechUsbMouse = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.2/2-1.2.4/"
               "2-1.2.4:1.0/input/input16/event9",
    /* name */ "Logitech USB Optical Mouse",
    /* phys */ "usb-0000:00:1d.0-1.2.4/input0",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "046d",
    /* product */ "c05a",
    /* version */ "0111",
    /* prop */ "0",
    /* ev */ "17",
    /* key */ "ff0000 0 0 0 0",
    /* rel */ "143",
    /* abs */ "0",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
};

// Captured from "Mimo Touch 2" Universal DisplayLink monitor.
const DeviceAbsoluteAxis kMimoTouch2TouchscreenAbsAxes[] = {
    {ABS_X, {0, 0, 2047, 0, 0, 0}},
    {ABS_Y, {0, 0, 2047, 0, 0, 0}},
};
const DeviceCapabilities kMimoTouch2Touchscreen = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.3/2-1.3.2/"
               "2-1.3.2:1.0/input/input15/event14",
    /* name */ "eGalax Inc. USB TouchController",
    /* phys */ "usb-0000:00:1d.0-1.3.2/input0",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "0eef",
    /* product */ "0001",
    /* version */ "0100",
    /* prop */ "0",
    /* ev */ "b",
    /* key */ "400 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "3",
    /* msc */ "0",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kMimoTouch2TouchscreenAbsAxes,
    arraysize(kMimoTouch2TouchscreenAbsAxes),
};

// Captured from Wacom Intuos Pen and Touch Small Tablet.
const DeviceAbsoluteAxis kWacomIntuosPtS_PenAbsAxes[] = {
    {ABS_X, {0, 0, 15200, 4, 0, 100}},
    {ABS_Y, {0, 0, 9500, 4, 0, 100}},
    {ABS_PRESSURE, {0, 0, 1023, 0, 0, 0}},
    {ABS_DISTANCE, {0, 0, 31, 0, 0, 0}},
};
const DeviceCapabilities kWacomIntuosPtS_Pen = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.2/2-1.2.3/"
               "2-1.2.3:1.0/input/input9/event9",
    /* name */ "Wacom Intuos PT S Pen",
    /* phys */ "",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "056a",
    /* product */ "0302",
    /* version */ "0100",
    /* prop */ "1",
    /* ev */ "b",
    /* key */ "1c03 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "3000003",
    /* msc */ "0",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kWacomIntuosPtS_PenAbsAxes,
    arraysize(kWacomIntuosPtS_PenAbsAxes),
};

// Captured from Wacom Intuos Pen and Touch Small Tablet.
const DeviceAbsoluteAxis kWacomIntuosPtS_FingerAbsAxes[] = {
    {ABS_X, {0, 0, 4096, 4, 0, 26}},
    {ABS_Y, {0, 0, 4096, 4, 0, 43}},
    {ABS_MT_SLOT, {0, 0, 15, 0, 0, 0}},
    {ABS_MT_TOUCH_MAJOR, {0, 0, 4096, 0, 0, 0}},
    {ABS_MT_TOUCH_MINOR, {0, 0, 4096, 0, 0, 0}},
    {ABS_MT_POSITION_X, {0, 0, 4096, 4, 0, 26}},
    {ABS_MT_POSITION_Y, {0, 0, 4096, 4, 0, 43}},
    {ABS_MT_TRACKING_ID, {0, 0, 65535, 0, 0, 0}},
};
const DeviceCapabilities kWacomIntuosPtS_Finger = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.2/2-1.2.3/"
               "2-1.2.3:1.1/input/input10/event10",
    /* name */ "Wacom Intuos PT S Finger",
    /* phys */ "",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "056a",
    /* product */ "0302",
    /* version */ "0100",
    /* prop */ "1",
    /* ev */ "2b",
    /* key */ "e520 630000 0 0 0 0",
    /* rel */ "0",
    /* abs */ "263800000000003",
    /* msc */ "0",
    /* sw */ "4000",
    /* led */ "0",
    /* ff */ "0",
    kWacomIntuosPtS_FingerAbsAxes,
    arraysize(kWacomIntuosPtS_FingerAbsAxes),
};

// Captured from Logitech Wireless Touch Keyboard K400.
const DeviceAbsoluteAxis kLogitechTouchKeyboardK400AbsAxes[] = {
    {ABS_VOLUME, {0, 1, 652, 0, 0, 0}},
};
const DeviceCapabilities kLogitechTouchKeyboardK400 = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.2/2-1.2.3/"
               "2-1.2.3:1.2/0003:046D:C52B.0006/input/input19/event17",
    /* name */ "Logitech Unifying Device. Wireless PID:4024",
    /* phys */ "usb-0000:00:1d.0-1.2.3:1",
    /* uniq */ "",
    /* bustype */ "001d",
    /* vendor */ "046d",
    /* product */ "4024",
    /* version */ "0111",
    /* prop */ "0",
    /* ev */ "12001f",
    /* key */ "3007f 0 0 483ffff17aff32d bf54444600000000 ffff0001 "
              "130f938b17c007 ffff7bfad9415fff febeffdfffefffff "
              "fffffffffffffffe",
    /* rel */ "1c3",
    /* abs */ "100000000",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "1f",
    /* ff */ "0",
    kLogitechTouchKeyboardK400AbsAxes,
    arraysize(kLogitechTouchKeyboardK400AbsAxes),
};

// Captured from Elo TouchSystems 2700 touchscreen.
const DeviceAbsoluteAxis kElo_TouchSystems_2700AbsAxes[] = {
    {ABS_X, {0, 0, 4095, 0, 0, 0}},
    {ABS_Y, {0, 0, 4095, 0, 0, 0}},
    {ABS_MISC, {0, 0, 256, 0, 0, 0}},
};
const DeviceCapabilities kElo_TouchSystems_2700 = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.3/2-1.3:1.0/"
               "input/input9/event9",
    /* name */ "Elo TouchSystems, Inc. Elo TouchSystems 2700 IntelliTouch(r) "
               "USB Touchmonitor Interface",
    /* phys */ "usb-0000:00:1d.0-1.3/input0",
    /* uniq */ "20A01347",
    /* bustype */ "0003",
    /* vendor */ "04e7",
    /* product */ "0020",
    /* version */ "0100",
    /* prop */ "0",
    /* ev */ "1b",
    /* key */ "10000 0 0 0 0",
    /* rel */ "0",
    /* abs */ "10000000003",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kElo_TouchSystems_2700AbsAxes,
    arraysize(kElo_TouchSystems_2700AbsAxes),
};

// Captured from Intel reference design: "Wilson Beach".
const DeviceAbsoluteAxis kWilsonBeachActiveStylusAbsAxes[] = {
    {ABS_X, {0, 0, 9600, 0, 0, 33}},
    {ABS_Y, {0, 0, 7200, 0, 0, 44}},
    {ABS_PRESSURE, {0, 0, 1024, 0, 0, 0}},
};
const DeviceCapabilities kWilsonBeachActiveStylus = {
  /* path */ "/sys/devices/pci0000:00/INT3433:00/i2c-1/"
    "i2c-NTRG0001:00/0018:1B96:0D03.0004/input/"
    "input11/event10",
  /* name */ "NTRG0001:00 1B96:0D03 Pen",
  /* phys */ "",
  /* uniq */ "",
  /* bustype */ "0018",
  /* vendor */ "1b96",
  /* product */ "0d03",
  /* version */ "0100",
  /* prop */ "0",
  /* ev */ "1b",
  /* key */ "c03 1 0 0 0 0",
  /* rel */ "0",
  /* abs */ "1000003",
  /* msc */ "10",
  /* sw */ "0",
  /* led */ "0",
  /* ff */ "0",
  kWilsonBeachActiveStylusAbsAxes,
  arraysize(kWilsonBeachActiveStylusAbsAxes),
};

// Captured from Eve Chromebook
const DeviceAbsoluteAxis kEveStylusAbsAxes[] = {
    {ABS_X, {0, 0, 25920, 0, 0, 100}},     {ABS_Y, {0, 0, 17280, 0, 0, 100}},
    {ABS_PRESSURE, {0, 0, 2047, 0, 0, 0}}, {ABS_TILT_X, {0, -90, 90, 0, 0, 57}},
    {ABS_TILT_Y, {0, -90, 90, 0, 0, 57}},  {ABS_MISC, {0, 0, 255, 0, 0, 0}},
};
const DeviceCapabilities kEveStylus = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:15.0/i2c_designware.0/i2c-6/"
    "i2c-WCOM50C1:00/0018:2D1F:5134.0001/input/input7/event7",
    /* name */ "WCOM50C1:00 2D1F:5134 Pen",
    /* phys */ "",
    /* uniq */ "",
    /* bustype */ "0018",
    /* vendor */ "2d1f",
    /* product */ "5134",
    /* version */ "0100",
    /* prop */ "0",
    /* ev */ "1b",
    /* key */ "1c03 1 0 0 0 0",
    /* rel */ "0",
    /* abs */ "1000d000003",
    /* msc */ "11",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kEveStylusAbsAxes,
    arraysize(kEveStylusAbsAxes),
};

const DeviceCapabilities kHammerKeyboard = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-7/1-7:1.0/0003:18D1:5030.0002/"
    "input/input10/event9",
    /* name */ "Google Inc. Hammer",
    /* phys */ "usb-0000:00:14.0-7/input0",
    /* uniq */ "410020000d57345436313920",
    /* bustype */ "0003",
    /* vendor */ "18d1",
    /* product */ "5030",
    /* version */ "0100",
    /* prop */ "0",
    /* ev */ "100013",
    /* key */
    "88 0 0 0 0 0 1000000000007 ff000000000007ff febeffdfffefffff "
    "fffffffffffffffe",
    /* rel */ "0",
    /* abs */ "0",
    /* msc */ "10",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
};

const DeviceAbsoluteAxis kHammerTouchpadAbsAxes[] = {
    {ABS_X, {0, 0, 2160, 0, 0, 21}},
    {ABS_Y, {0, 0, 1080, 0, 0, 14}},
    {ABS_PRESSURE, {0, 0, 255, 0, 0, 0}},
    {ABS_MT_SLOT, {0, 0, 9, 0, 0, 0}},
    {ABS_MT_TOUCH_MAJOR, {0, 0, 255, 0, 0, 3}},
    {ABS_MT_TOUCH_MINOR, {0, 0, 255, 0, 0, 3}},
    {ABS_MT_ORIENTATION, {0, 0, 1, 0, 0, 0}},
    {ABS_MT_POSITION_X, {0, 0, 2160, 0, 0, 21}},
    {ABS_MT_POSITION_Y, {0, 0, 1080, 0, 0, 14}},
    {ABS_MT_TRACKING_ID, {0, 0, 65535, 0, 0, 0}},
    {ABS_MT_PRESSURE, {0, 0, 255, 0, 0, 0}},
};
const DeviceCapabilities kHammerTouchpad = {
    /* path */
    "/sys/devices/pci0000:00/0000:00:14.0/usb1/1-7/1-7:1.2/0003:18D1:5030.0003/"
    "input/input11/event10",
    /* name */ "Google Inc. Hammer Touchpad",
    /* phys */ "usb-0000:00:14.0-7/input2",
    /* uniq */ "410020000d57345436313920",
    /* bustype */ "0003",
    /* vendor */ "18d1",
    /* product */ "5030",
    /* version */ "0100",
    /* prop */ "5",
    /* ev */ "1b",
    /* key */ "e520 10000 0 0 0 0",
    /* rel */ "0",
    /* abs */ "673800001000003",
    /* msc */ "20",
    /* sw */ "0",
    /* led */ "0",
    /* ff */ "0",
    kHammerTouchpadAbsAxes,
    arraysize(kHammerTouchpadAbsAxes),
};

// NB: Please use the capture_device_capabilities.py script to add more
// test data here. This will help ensure the data matches what the kernel
// reports for a real device and is entered correctly.
//
// For Chrome OS, you can run the script by installing a test image and running:
//   DEVICE_IP=<your device IP>
//   cd ui/events/ozone/evdev/
//   scp capture_device_capabilities.py "root@${DEVICE_IP}:/tmp/"
//   ssh "root@${DEVICE_IP}" /tmp/capture_device_capabilities.py

bool CapabilitiesToDeviceInfo(const DeviceCapabilities& capabilities,
                              EventDeviceInfo* devinfo) {
  std::vector<unsigned long> ev_bits;
  if (!ParseBitfield(capabilities.ev, EV_CNT, &ev_bits))
    return false;
  devinfo->SetEventTypes(&ev_bits[0], ev_bits.size());

  std::vector<unsigned long> key_bits;
  if (!ParseBitfield(capabilities.key, KEY_CNT, &key_bits))
    return false;
  devinfo->SetKeyEvents(&key_bits[0], key_bits.size());

  std::vector<unsigned long> rel_bits;
  if (!ParseBitfield(capabilities.rel, REL_CNT, &rel_bits))
    return false;
  devinfo->SetRelEvents(&rel_bits[0], rel_bits.size());

  std::vector<unsigned long> abs_bits;
  if (!ParseBitfield(capabilities.abs, ABS_CNT, &abs_bits))
    return false;
  devinfo->SetAbsEvents(&abs_bits[0], abs_bits.size());

  std::vector<unsigned long> msc_bits;
  if (!ParseBitfield(capabilities.msc, MSC_CNT, &msc_bits))
    return false;
  devinfo->SetMscEvents(&msc_bits[0], msc_bits.size());

  std::vector<unsigned long> led_bits;
  if (!ParseBitfield(capabilities.led, LED_CNT, &led_bits))
    return false;
  devinfo->SetLedEvents(&led_bits[0], led_bits.size());

  std::vector<unsigned long> prop_bits;
  if (!ParseBitfield(capabilities.prop, INPUT_PROP_CNT, &prop_bits))
    return false;
  devinfo->SetProps(&prop_bits[0], prop_bits.size());

  for (size_t i = 0; i < capabilities.abs_axis_count; ++i) {
    const DeviceAbsoluteAxis& axis = capabilities.abs_axis[i];
    devinfo->SetAbsInfo(axis.code, axis.absinfo);
  }

  size_t slots = devinfo->GetAbsMtSlotCount();
  std::vector<int32_t> zero_slots(slots, 0);
  std::vector<int32_t> minus_one_slots(slots, -1);
  for (int code = EVDEV_ABS_MT_FIRST; code <= EVDEV_ABS_MT_LAST; ++code) {
    if (!devinfo->HasAbsEvent(code))
      continue;
    if (code == ABS_MT_TRACKING_ID)
      devinfo->SetAbsMtSlots(code, minus_one_slots);
    else
      devinfo->SetAbsMtSlots(code, zero_slots);
  }

  input_id id = {};
  sscanf(capabilities.vendor, "%" SCNx16, &id.vendor);
  sscanf(capabilities.product, "%" SCNx16, &id.product);
  sscanf(capabilities.bustype, "%" SCNx16, &id.bustype);
  sscanf(capabilities.version, "%" SCNx16, &id.version);
  devinfo->SetId(id);
  devinfo->SetDeviceType(EventDeviceInfo::GetInputDeviceTypeFromId(id));

  return true;
}

}  // namespace ui
