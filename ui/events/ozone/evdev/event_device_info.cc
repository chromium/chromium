// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/ozone/evdev/event_device_info.h"

#include <linux/input.h>

#include <cstring>

#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/fixed_array.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/ozone/evdev/keyboard_mouse_combo_device_metrics.h"
#include "ui/events/ozone/features.h"

#if !defined(EVIOCGMTSLOTS)
#define EVIOCGMTSLOTS(len) _IOC(_IOC_READ, 'E', 0x0a, len)
#endif

#ifndef INPUT_PROP_HAPTICPAD
#define INPUT_PROP_HAPTICPAD 0x07
#endif

namespace ui {

namespace {

// USB vendor and product strings are pragmatically limited to 126
// characters each, so device names more than twice that should be
// unusual.
const size_t kMaximumDeviceNameLength = 256;

struct DeviceId {
  uint16_t vendor;
  uint16_t product_id;
  constexpr bool operator<(const DeviceId& other) const {
    return vendor == other.vendor ? product_id < other.product_id
                                  : vendor < other.vendor;
  }
};

constexpr auto kKeyboardAllowlist = base::MakeFixedFlatSet<DeviceId>({
    {0x2516, 0x0016},  // CM Storm Quickfire Pro Ultimate
});

constexpr auto kKeyboardBlocklist = base::MakeFixedFlatSet<DeviceId>({
    {0x0111, 0x1830},  // SteelSeries Rival 3 Wireless (Bluetooth)
    {0x0111, 0x183a},  // SteelSeries Aerox 3 Wireless (Bluetooth)
    {0x0111, 0x1854},  // SteelSeries Aerox 5 Wireless (Bluetooth)
    {0x0111, 0x185a},  // SteelSeries Aerox 9 Wireless (Bluetooth)
    {0x03f0, 0x0b97},  // HyperX Pulsefire Haste 2 Gaming Mouse
    {0x03f0, 0x4e41},  // HP OMEN Vector Wireless Mouse
    {0x03f0, 0xa407},  // HP X4000 Wireless Mouse
    {0x045e, 0x0745},  // Microsoft Wireless Mobile Mouse 6000
    {0x045e, 0x07a2},  // Microsoft Sculpt Comfort Mouse
    {0x045e, 0x0821},  // Microsoft Surface Precision Mouse
    {0x045e, 0x0827},  // Microsoft Modern Mobile Mouse
    {0x045e, 0x082a},  // Microsoft Pro IntelliMouse
    {0x045e, 0x082f},  // Microsoft Bluetooth Mouse
    {0x045e, 0x0845},  // Microsoft Ocean Plastic Mouse
    {0x045e, 0x0932},  // Microsoft Surface Arc Mouse
    {0x045e, 0x095d},  // Microsoft Surface Mobile Mouse
    {0x045e, 0x0b05},  // Xbox One Elite Series 2 gamepad
    {0x046d, 0x4026},  // Logitech T400
    {0x046d, 0x404a},  // Logitech MX Anywhere 2 (Unifying)
    {0x046d, 0x405e},  // Logitech M720 Triathlon (Unifying)
    {0x046d, 0x4069},  // Logitech MX Master 2S (Unifying) // nocheck
    {0x046d, 0x406b},  // Logitech M585 (Unifying)
    {0x046d, 0x406f},  // Logitech MX Ergo
    {0x046d, 0x4072},  // Logitech MX Anywhere 2 (Unifying)
    {0x046d, 0x407b},  // Logitech MX Vertical
    {0x046d, 0x4080},  // Logitech Pebble M350
    {0x046d, 0x4082},  // Logitech MX Master 3 (Unifying)
    {0x046d, 0xb00d},  // Logitech T630 Ultrathin
    {0x046d, 0xb011},  // Logitech M558
    {0x046d, 0xb012},  // Logitech MX Master (Bluetooth) // nocheck
    {0x046d, 0xb013},  // Logitech MX Anywhere 2 (Bluetooth)
    {0x046d, 0xb014},  // Logitech M337
    {0x046d, 0xb015},  // Logitech M720 Triathlon (Bluetooth)
    {0x046d, 0xb016},  // Logitech M535
    {0x046d, 0xb017},  // Logitech MX Master / Anywhere 2 (Bluetooth) // nocheck
    {0x046d, 0xb019},  // Logitech MX Master 2S (Bluetooth) // nocheck
    {0x046d, 0xb01a},  // Logitech MX Anywhere 2S (Bluetooth)
    {0x046d, 0xb01b},  // Logitech M585/M590 (Bluetooth)
    {0x046d, 0xb01c},  // Logitech G603 Lightspeed Gaming Mouse (Bluetooth)
    {0x046d, 0xb01e},  // Logitech MX Master (Bluetooth) // nocheck
    {0x046d, 0xb01f},  // Logitech MX Anywhere 2 (Bluetooth)
    {0x046d, 0xb023},  // Logitech MX Master 3 (Bluetooth) // nocheck
    {0x046d, 0xb024},  // Logitech G604 Lightspeed Gaming Mouse (Bluetooth)
    {0x046d, 0xb503},  // Logitech Spotlight Presentation Remote (Bluetooth)
    {0x046d, 0xb505},  // Logitech R500 (Bluetooth)
    {0x046d, 0xc087},  // Logitech G703
    {0x046d, 0xc088},  // Logitech G Pro Wireless (USB)
    {0x046d, 0xc08b},  // Logitech G502 Hero
    {0x046d, 0xc08c},  // Logitech G Pro Gaming Mouse (Wired)
    {0x046d, 0xc091},  // Logitech G903
    {0x046d, 0xc092},  // Logitech G203 LIGHTSYNC
    {0x046d, 0xc093},  // Logitech M500s
    {0x046d, 0xc094},  // Logitech G Pro Wireless X Superlight (USB)
    {0x046d, 0xc09d},  // Logitech G203
    {0x046d, 0xc53e},  // Logitech Spotlight Presentation Remote (USB dongle)
    {0x04b4, 0x121f},  // SteelSeries Ikari
    {0x056e, 0x0134},  // Elecom Enelo IR LED Mouse 350
    {0x056e, 0x0141},  // Elecom EPRIM Blue LED 5 button mouse 228
    {0x056e, 0x0159},  // Elecom Blue LED Mouse 203
    {0x05e0, 0x1200},  // Symbol Technologies / Zebra LS2208 barcode scanner
    {0x093a, 0x2533},  // CyberPower Mouse
    {0x0951, 0x16d3},  // HyperX Pulsefire Surge
    {0x0951, 0x16de},  // HyperX Pulsefire Core
    {0x0951, 0x16e2},  // HyperX Pulsefire Dart
    {0x0951, 0x1727},  // HyperX Pulsefire Haste Gaming Mouse
    {0x0b05, 0x1949},  // ASUS ROG Strix Impact II
    {0x0b33, 0x3022},  // Contour Design RollerMouse Pro
    {0x1038, 0x0470},  // SteelSeries Reaper Edge
    {0x1038, 0x0471},  // SteelSeries Rival Rescuer
    {0x1038, 0x0472},  // SteelSeries Rival 150 net café
    {0x1038, 0x0473},  // SteelSeries Sensei SP
    {0x1038, 0x0475},  // SteelSeries Rival 160 retail
    {0x1038, 0x0777},  // SteelSeries MO3
    {0x1038, 0x1300},  // SteelSeries Kinzu
    {0x1038, 0x1310},  // SteelSeries MO4
    {0x1038, 0x1311},  // SteelSeries MO4v2
    {0x1038, 0x1320},  // SteelSeries MO3v2
    {0x1038, 0x1330},  // SteelSeries MO5
    {0x1038, 0x1332},  // SteelSeries MO5 (Dongle)
    {0x1038, 0x1356},  // SteelSeries Sensei Dark EDG
    {0x1038, 0x1358},  // SteelSeries Sensei Dark Snake
    {0x1038, 0x135a},  // SteelSeries Sensei Dell Alienware
    {0x1038, 0x1360},  // SteelSeries Xai
    {0x1038, 0x1361},  // SteelSeries Sensei
    {0x1038, 0x1362},  // SteelSeries Sensei Raw Diablo III Mouse
    {0x1038, 0x1364},  // SteelSeries Kana
    {0x1038, 0x1366},  // SteelSeries Kinzu 2
    {0x1038, 0x1369},  // SteelSeries Sensei Raw
    {0x1038, 0x136b},  // SteelSeries MLG Sensei
    {0x1038, 0x136d},  // SteelSeries Sensei Raw: GW2
    {0x1038, 0x136f},  // SteelSeries Sensei Raw: CoD
    {0x1038, 0x1370},  // SteelSeries Sensei Master
    {0x1038, 0x1372},  // SteelSeries Sensei Master (Hub Controller)
    {0x1038, 0x1373},  // SteelSeries Sensei Master (Flash Drive Controller)
    {0x1038, 0x1374},  // SteelSeries Kana: CS:GO
    {0x1038, 0x1376},  // SteelSeries Kana: DOTA
    {0x1038, 0x1378},  // SteelSeries Kinzu v2.1
    {0x1038, 0x137a},  // SteelSeries Kana Pro
    {0x1038, 0x137c},  // SteelSeries Wireless Sensei
    {0x1038, 0x137e},  // SteelSeries Wireless Sensei (Charge Stand)
    {0x1038, 0x1380},  // SteelSeries World of Tank mouse
    {0x1038, 0x1382},  // SteelSeries Sims Mouse
    {0x1038, 0x1384},  // SteelSeries Rival
    {0x1038, 0x1386},  // SteelSeries SIMS 4 mouse
    {0x1038, 0x1388},  // SteelSeries Kinzu v3 Mouse
    {0x1038, 0x1390},  // SteelSeries Sensei Raw Heroes of the Storm Mouse
    {0x1038, 0x1392},  // SteelSeries Rival DOTA 2
    {0x1038, 0x1394},  // SteelSeries Rival 300 CS:GO Fade Edition
    {0x1038, 0x1396},  // SteelSeries Rival 300 Gaming Mouse
    {0x1038, 0x1700},  // SteelSeries Rival 700
    {0x1038, 0x1701},  // SteelSeries Rival 700 (Basic)
    {0x1038, 0x1702},  // SteelSeries Rival 100 Gaming Mouse (ELM4 - A)
    {0x1038, 0x1704},  // SteelSeries Rival 95 PC BANG
    {0x1038, 0x1705},  // SteelSeries Rival 100 For Alienware
    {0x1038, 0x1706},  // SteelSeries Rival 95
    {0x1038, 0x1707},  // SteelSeries Rival 95 MSI edition
    {0x1038, 0x1708},  // SteelSeries Rival 100 Gaming Mouse (PC Bang)
    {0x1038, 0x1709},  // SteelSeries Rival 50 MSI edition
    {0x1038, 0x170a},  // SteelSeries Rival 100 Dell China
    {0x1038, 0x170b},  // SteelSeries Rival 100 DOTA 2 Mouse
    {0x1038, 0x170c},  // SteelSeries Rival 100 DOTA 2 Mouse (Lenovo)
    {0x1038, 0x170d},  // SteelSeries Rival 100 World of Tanks Mouse
    {0x1038, 0x170e},  // SteelSeries Rival 500 (MBM)
    {0x1038, 0x170f},  // SteelSeries Rival 500 (Basic)
    {0x1038, 0x1710},  // SteelSeries Rival 300 Gaming Mouse
    {0x1038, 0x1712},  // SteelSeries Rival 300 Fallout 4 Gaming Mouse
    {0x1038, 0x1714},  // SteelSeries Rival 300 Predator Gaming Mouse
    {0x1038, 0x1716},  // SteelSeries Rival 300 CS:GO Fade Edition
    {0x1038, 0x1718},  // SteelSeries Rival 300 HP Omen
    {0x1038, 0x171a},  // SteelSeries Rival 300 CS:GO Hyperbeast Edition
    {0x1038, 0x171c},  // SteelSeries Rival 300 Evil Geniuses Edition
    {0x1038, 0x171e},  // SteelSeries Rival 310 CSGO Howl
    {0x1038, 0x171f},  // SteelSeries Rival 310 CSGO Howl (Basic)
    {0x1038, 0x1720},  // SteelSeries Rival 310
    {0x1038, 0x1721},  // SteelSeries Rival 310 (Basic)
    {0x1038, 0x1722},  // SteelSeries Sensei 310
    {0x1038, 0x1723},  // SteelSeries Sensei 310  (Basic)
    {0x1038, 0x1724},  // SteelSeries Rival 600
    {0x1038, 0x1725},  // SteelSeries Rival 600 (Basic)
    {0x1038, 0x1726},  // SteelSeries Rival 650 Wireless
    {0x1038, 0x1727},  // SteelSeries Rival 650 Wireless (Basic)
    {0x1038, 0x1729},  // SteelSeries Rival 110 Gaming Mouse
    {0x1038, 0x172b},  // SteelSeries Rival 650 Wireless (Wired)
    {0x1038, 0x172c},  // SteelSeries Rival 650 Wireless (Basic for wired)
    {0x1038, 0x172d},  // SteelSeries Rival 110 (Dell)
    {0x1038, 0x172e},  // SteelSeries Rival 600 Dota 2
    {0x1038, 0x172f},  // SteelSeries Rival 600 Dota 2 (Basic)
    {0x1038, 0x1730},  // SteelSeries Rival 710
    {0x1038, 0x1731},  // SteelSeries Rival 710 (Basic)
    {0x1038, 0x1736},  // SteelSeries Rival 310 PUBG Edition
    {0x1038, 0x1737},  // SteelSeries Rival 310 PUBG Edition (Basic)
    {0x1038, 0x1800},  // SteelSeries Sensei Raw Optical
    {0x1038, 0x1801},  // SteelSeries Sensei Raw Optical (Basic)
    {0x1038, 0x1802},  // SteelSeries Sensei Raw Optical RGB
    {0x1038, 0x1803},  // SteelSeries Sensei Raw Optical RGB (Basic)
    {0x1038, 0x1810},  // SteelSeries Rival 300S
    {0x1038, 0x1812},  // SteelSeries Rival 300S Dell Silver
    {0x1038, 0x1814},  // SteelSeries Rival 105 (Kana v3) Gaming Mouse
    {0x1038, 0x1816},  // SteelSeries Rival 106 Gaming Mouse
    {0x1038, 0x1818},  // SteelSeries Rival 610 Wireless
    {0x1038, 0x1819},  // SteelSeries Rival 610 Wireless (Basic)
    {0x1038, 0x181a},  // SteelSeries Rival 610 Wireless (Wired)
    {0x1038, 0x181b},  // SteelSeries Rival 610 Wireless (Basic for wired)
    {0x1038, 0x181c},  // SteelSeries Rival 310 Wireless
    {0x1038, 0x181d},  // SteelSeries Rival 310 Wireless (Basic)
    {0x1038, 0x181e},  // SteelSeries Rival 310 Wireless (Wired)
    {0x1038, 0x181f},  // SteelSeries Rival 310 Wireless (Basic for wired)
    {0x1038, 0x1820},  // SteelSeries Rival 610
    {0x1038, 0x1821},  // SteelSeries Rival 610 (Basic)
    {0x1038, 0x1822},  // SteelSeries Sensei 610
    {0x1038, 0x1823},  // SteelSeries Sensei 610 (Basic)
    {0x1038, 0x1824},  // SteelSeries Rival 3
    {0x1038, 0x1826},  // SteelSeries Sensei Raw Optical RGB v2
    {0x1038, 0x1827},  // SteelSeries Sensei Raw Optical RGB v2 (Basic)
    {0x1038, 0x1828},  // SteelSeries Radical Wireless
    {0x1038, 0x1829},  // SteelSeries Radical Wireless (Basic)
    {0x1038, 0x182a},  // SteelSeries Prime Rainbow Six Edition
    {0x1038, 0x182b},  // SteelSeries Prime Rainbow Six Edition (Basic)
    {0x1038, 0x182c},  // SteelSeries Prime+
    {0x1038, 0x182d},  // SteelSeries Prime+ (Basic)
    {0x1038, 0x182e},  // SteelSeries Prime
    {0x1038, 0x182f},  // SteelSeries Prime (Basic)
    {0x1038, 0x1830},  // SteelSeries Rival 3 Wireless
    {0x1038, 0x1831},  // SteelSeries Rival 3 Wireless (Basic)
    {0x1038, 0x1832},  // SteelSeries Sensei Ten
    {0x1038, 0x1833},  // SteelSeries Sensei Ten (Basic)
    {0x1038, 0x1834},  // SteelSeries Sensei Ten Neon Rider Edition
    {0x1038, 0x1835},  // SteelSeries Sensei Ten Neon Rider Edition (Basic)
    {0x1038, 0x1836},  // SteelSeries Aerox 3
    {0x1038, 0x1838},  // SteelSeries Aerox 3 Wireless (Dongle)
    {0x1038, 0x1839},  // SteelSeries Aerox 3 Wireless (Basic for dongle)
    {0x1038, 0x183a},  // SteelSeries Aerox 3 Wireless (Wired)
    {0x1038, 0x183b},  // SteelSeries Aerox 3 Wireless (Basic for wired)
    {0x1038, 0x183c},  // SteelSeries Rival 5
    {0x1038, 0x183d},  // SteelSeries Rival 5 (Basic)
    {0x1038, 0x183e},  // SteelSeries Rival 5 Destiny 2
    {0x1038, 0x183f},  // SteelSeries Rival 5 Destiny 2 (Basic)
    {0x1038, 0x1840},  // SteelSeries Prime Wireless (Dongle)
    {0x1038, 0x1841},  // SteelSeries Prime Wireless (Basic for dongle)
    {0x1038, 0x1842},  // SteelSeries Prime Wireless (Wired)
    {0x1038, 0x1843},  // SteelSeries Prime Wireless (Basic for wired)
    {0x1038, 0x1848},  // SteelSeries Prime Mini Wireless (Dongle)
    {0x1038, 0x184a},  // SteelSeries Prime Mini Wireless (Wired)
    {0x1038, 0x184c},  // SteelSeries Rival 3 (NVIDIA Support - Standard)
    {0x1038, 0x184d},  // SteelSeries Prime Mini
    {0x1038, 0x1850},  // SteelSeries Aerox 5
    {0x1038, 0x1852},  // SteelSeries Aerox 5 Wireless (Dongle)
    {0x1038, 0x1854},  // SteelSeries Aerox 5 Wireless (Wired)
    {0x1038, 0x1856},  // SteelSeries Prime CS:GO Neo Noir
    {0x1038, 0x1858},  // SteelSeries Aerox 9 WL (Dongle)
    {0x1038, 0x185a},  // SteelSeries Aerox 9 WL (Wired)
    {0x1050, 0x0010},  // Yubico.com Yubikey
    {0x1050, 0x0407},  // Yubico.com Yubikey 4 OTP+U2F+CCID
    {0x12cf, 0x0490},  // Acer Cestus 325
    {0x1532, 0x005c},  // Razer DeathAdder Elite
    {0x1532, 0x0062},  // Razer Atheris
    {0x1532, 0x0071},  // Razer DeathAdder Essential - White
    {0x1532, 0x0078},  // Razer Viper
    {0x1532, 0x007a},  // Razer Viper Ultimate (Wired)
    {0x1532, 0x007b},  // Razer Viper Ultimate (Wireless)
    {0x1532, 0x007d},  // Razer DeathAdder V2 Pro
    {0x1532, 0x0083},  // Razer Basilsk X HyperSpeed
    {0x1532, 0x008a},  // Razer Viper Mini
    {0x1532, 0x0094},  // Razer Orochi V2 (USB dongle)
    {0x1532, 0x0098},  // Razer DeathAdder Essential
    {0x1532, 0x009a},  // Razer Pro Click Mini (Dongle)
    {0x1532, 0x009b},  // Razer Pro Click Mini (Bluetooth)
    {0x1532, 0x00b6},  // Razer DeathAdder V3 Pro
    {0x17ef, 0x60be},  // Lenovo Legion M200 RGB Gaming Mouse
    {0x17ef, 0x60e4},  // Lenovo Legion M300 RGB Gaming Mouse
    {0x17ef, 0x6123},  // Lenovo USB-C Wired Compact Mouse
    {0x1b1c, 0x1b7a},  // Corsair Sabre Pro Champion Gaming Mouse
    {0x1b1c, 0x1b94},  // Corsair Katar Pro Wireless (USB dongle)
    {0x1b1c, 0x1b9e},  // Corsair M65 RGB
    {0x1bae, 0x1b1c},  // Corsair Katar Pro Wireless (Bluetooth)
    {0x1b1c, 0x1bac},  // Corsair Katar Pro
    {0x1bcf, 0x08a0},  // Kensington Pro Fit Full-size
    {0x1e7d, 0x2c88},  // ROCCAT Kone Pro
    {0x1e7d, 0x2c8a},  // ROCCAT Kone Pro Air
    {0x1e7d, 0x2ca6},  // ROCCAT Burst Pro Air (USB dongle)
    {0x1e7d, 0x2cab},  // ROCCAT Burst Pro Air
    {0x2201, 0x0100},  // AirTurn PEDpro
    {0x256c, 0x006d},  // Huion HS64
    {0x258a, 0x1007},  // Acer Cestus 330
    {0x2717, 0x003b},  // Xiaomi Mi Portable Mouse
    {0x28bd, 0x0914},  // XP-Pen Star G640
    {0x28bd, 0x091f},  // XP-Pen Artist 12 Pro
    {0x28bd, 0x0928},  // XP-Pen Deco mini7W
    {0x5043, 0x5442},  // Ploopy Trackball
});

constexpr DeviceId kStylusButtonDevices[] = {
    {0x413c, 0x81d5},  // Dell Active Pen PN579X
};

constexpr DeviceId kHeatmapSupportedDevices[] = {
    {0x04f3, 0x4222},  // Rex
};

// Certain devices need to be forced to use libinput in place of
// evdev/libgestures
constexpr DeviceId kForceLibinputlist[] = {
    {0x0002, 0x000e},  // HP Stream 14 touchpad
    {0x044e, 0x120a},  // Dell Latitude 3480 touchpad
};

bool IsForceLibinput(const EventDeviceInfo& devinfo) {
  for (auto entry : kForceLibinputlist) {
    if (devinfo.vendor_id() == entry.vendor &&
        devinfo.product_id() == entry.product_id) {
      return true;
    }
  }

  return false;
}

const uint16_t kSteelSeriesBluetoothVendorId = 0x0111;
const uint16_t kSteelSeriesStratusDuoBluetoothProductId = 0x1431;
const uint16_t kSteelSeriesStratusPlusBluetoothProductId = 0x1434;

const uint16_t kFlossVirtualSuspendVendorId = 0x0000;
const uint16_t kFlossVirtualSuspendProductId = 0x0000;
const char kFlossVirtualSuspendName[] = "VIRTUAL_SUSPEND_UHID";

bool GetEventBits(int fd,
                  const base::FilePath& path,
                  unsigned int type,
                  void* buf,
                  unsigned int size) {
  if (ioctl(fd, EVIOCGBIT(type, size), buf) < 0) {
    PLOG(ERROR) << "Failed EVIOCGBIT (path=" << path.value() << " type=" << type
                << " size=" << size << ")";
    return false;
  }

  return true;
}

bool GetPropBits(int fd,
                 const base::FilePath& path,
                 void* buf,
                 unsigned int size) {
  if (ioctl(fd, EVIOCGPROP(size), buf) < 0) {
    PLOG(ERROR) << "Failed EVIOCGPROP (path=" << path.value() << ")";
    return false;
  }

  return true;
}

bool GetAbsInfo(int fd,
                const base::FilePath& path,
                int code,
                struct input_absinfo* absinfo) {
  if (ioctl(fd, EVIOCGABS(code), absinfo)) {
    PLOG(ERROR) << "Failed EVIOCGABS (path=" << path.value() << " code=" << code
                << ")";
    return false;
  }
  return true;
}

bool GetDeviceName(int fd, const base::FilePath& path, std::string* name) {
  char device_name[kMaximumDeviceNameLength];
  if (ioctl(fd, EVIOCGNAME(kMaximumDeviceNameLength - 1), &device_name) < 0) {
    PLOG(INFO) << "Failed EVIOCGNAME (path=" << path.value() << ")";
    return false;
  }
  *name = device_name;
  return true;
}

bool GetDeviceIdentifiers(int fd, const base::FilePath& path, input_id* id) {
  *id = {};
  if (ioctl(fd, EVIOCGID, id) < 0) {
    PLOG(INFO) << "Failed EVIOCGID (path=" << path.value() << ")";
    return false;
  }
  return true;
}

void GetDevicePhysInfo(int fd, const base::FilePath& path, std::string* phys) {
  char device_phys[kMaximumDeviceNameLength];
  if (ioctl(fd, EVIOCGPHYS(kMaximumDeviceNameLength - 1), &device_phys) < 0) {
    PLOG(INFO) << "Failed EVIOCGPHYS (path=" << path.value() << ")";
    return;
  }
  *phys = device_phys;
}

// |request| needs to be the equivalent to:
// struct input_mt_request_layout {
//   uint32_t code;
//   int32_t values[num_slots];
// };
//
// |size| is num_slots + 1 (for code).
void GetSlotValues(int fd,
                   const base::FilePath& path,
                   int32_t* request,
                   unsigned int size) {
  size_t data_size = size * sizeof(*request);
  if (ioctl(fd, EVIOCGMTSLOTS(data_size), request) < 0) {
    PLOG(ERROR) << "Failed EVIOCGMTSLOTS (code=" << request[0]
                << " path=" << path.value() << ")";
  }
}

void AssignBitset(const unsigned long* src,
                  size_t src_len,
                  unsigned long* dst,
                  size_t dst_len) {
  memcpy(dst, src, std::min(src_len, dst_len) * sizeof(unsigned long));
  if (src_len < dst_len)
    memset(&dst[src_len], 0, (dst_len - src_len) * sizeof(unsigned long));
}

bool IsDenylistedAbsoluteMouseDevice(const input_id& id) {
  static constexpr struct {
    uint16_t vid;
    uint16_t pid;
  } kUSBLegacyDenyListedDevices[] = {
      {0x222a, 0x0001},  // ILITEK ILITEK-TP
  };

  for (size_t i = 0; i < std::size(kUSBLegacyDenyListedDevices); ++i) {
    if (id.vendor == kUSBLegacyDenyListedDevices[i].vid &&
        id.product == kUSBLegacyDenyListedDevices[i].pid) {
      return true;
    }
  }

  return false;
}

}  // namespace

EventDeviceInfo::EventDeviceInfo()
    : ev_bits_{},
      key_bits_{},
      rel_bits_{},
      abs_bits_{},
      msc_bits_{},
      sw_bits_{},
      led_bits_{},
      prop_bits_{},
      ff_bits_{},
      abs_info_{} {}

EventDeviceInfo::~EventDeviceInfo() {}

bool EventDeviceInfo::Initialize(int fd, const base::FilePath& path) {
  if (!GetEventBits(fd, path, 0, ev_bits_.data(), sizeof(ev_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_KEY, key_bits_.data(), sizeof(key_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_REL, rel_bits_.data(), sizeof(rel_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_ABS, abs_bits_.data(), sizeof(abs_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_MSC, msc_bits_.data(), sizeof(msc_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_SW, sw_bits_.data(), sizeof(sw_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_LED, led_bits_.data(), sizeof(led_bits_)))
    return false;

  if (!GetEventBits(fd, path, EV_FF, ff_bits_.data(), sizeof(ff_bits_)))
    return false;

  if (!GetPropBits(fd, path, prop_bits_.data(), sizeof(prop_bits_)))
    return false;

  for (unsigned int i = 0; i < ABS_CNT; ++i)
    if (HasAbsEvent(i))
      if (!GetAbsInfo(fd, path, i, &abs_info_[i]))
        return false;

  int max_num_slots = GetAbsMtSlotCount();

  // |request| is MT code + slots.
  base::FixedArray<int32_t> request(max_num_slots + 1);
  int32_t& request_code = request.front();
  for (unsigned int i = EVDEV_ABS_MT_FIRST; i <= EVDEV_ABS_MT_LAST; ++i) {
    if (!HasAbsEvent(i))
      continue;

    memset(request.data(), 0, request.memsize());
    request_code = i;
    GetSlotValues(fd, path, request.data(), max_num_slots + 1);

    std::vector<int32_t>* slots = &slot_values_[i - EVDEV_ABS_MT_FIRST];
    slots->assign(request.begin() + 1, request.begin() + 1 + max_num_slots);
  }

  if (!GetDeviceName(fd, path, &name_))
    return false;

  if (!GetDeviceIdentifiers(fd, path, &input_id_))
    return false;

  GetDevicePhysInfo(fd, path, &phys_);

  device_type_ = GetInputDeviceTypeFromId(input_id_);
  if (device_type_ == InputDeviceType::INPUT_DEVICE_UNKNOWN)
    device_type_ = GetInputDeviceTypeFromPath(path);

  return true;
}

void EventDeviceInfo::SetEventTypes(const unsigned long* ev_bits, size_t len) {
  AssignBitset(ev_bits, len, ev_bits_.data(), ev_bits_.size());
}

void EventDeviceInfo::SetKeyEvents(const unsigned long* key_bits, size_t len) {
  AssignBitset(key_bits, len, key_bits_.data(), key_bits_.size());
}

void EventDeviceInfo::SetRelEvents(const unsigned long* rel_bits, size_t len) {
  AssignBitset(rel_bits, len, rel_bits_.data(), rel_bits_.size());
}

void EventDeviceInfo::SetAbsEvents(const unsigned long* abs_bits, size_t len) {
  AssignBitset(abs_bits, len, abs_bits_.data(), abs_bits_.size());
}

void EventDeviceInfo::SetMscEvents(const unsigned long* msc_bits, size_t len) {
  AssignBitset(msc_bits, len, msc_bits_.data(), msc_bits_.size());
}

void EventDeviceInfo::SetSwEvents(const unsigned long* sw_bits, size_t len) {
  AssignBitset(sw_bits, len, sw_bits_.data(), sw_bits_.size());
}

void EventDeviceInfo::SetLedEvents(const unsigned long* led_bits, size_t len) {
  AssignBitset(led_bits, len, led_bits_.data(), led_bits_.size());
}

void EventDeviceInfo::SetFfEvents(const unsigned long* ff_bits, size_t len) {
  AssignBitset(ff_bits, len, ff_bits_.data(), ff_bits_.size());
}

void EventDeviceInfo::SetProps(const unsigned long* prop_bits, size_t len) {
  AssignBitset(prop_bits, len, prop_bits_.data(), prop_bits_.size());
}

void EventDeviceInfo::SetAbsInfo(unsigned int code,
                                 const input_absinfo& abs_info) {
  if (code > ABS_MAX)
    return;

  memcpy(&abs_info_[code], &abs_info, sizeof(abs_info));
}

void EventDeviceInfo::SetAbsMtSlots(unsigned int code,
                                    const std::vector<int32_t>& values) {
  DCHECK_EQ(GetAbsMtSlotCount(), values.size());
  int index = code - EVDEV_ABS_MT_FIRST;
  if (index < 0 || index >= EVDEV_ABS_MT_COUNT)
    return;
  slot_values_[index] = values;
}

void EventDeviceInfo::SetAbsMtSlot(unsigned int code,
                                   unsigned int slot,
                                   uint32_t value) {
  int index = code - EVDEV_ABS_MT_FIRST;
  if (index < 0 || index >= EVDEV_ABS_MT_COUNT)
    return;
  slot_values_[index][slot] = value;
}

void EventDeviceInfo::SetDeviceType(InputDeviceType type) {
  device_type_ = type;
}

void EventDeviceInfo::SetId(input_id id) {
  input_id_ = id;
}
void EventDeviceInfo::SetName(const std::string& name) {
  name_ = name;
}

bool EventDeviceInfo::HasEventType(unsigned int type) const {
  if (type > EV_MAX)
    return false;
  return EvdevBitIsSet(ev_bits_.data(), type);
}

bool EventDeviceInfo::HasKeyEvent(unsigned int code) const {
  if (code > KEY_MAX)
    return false;
  return EvdevBitIsSet(key_bits_.data(), code);
}

bool EventDeviceInfo::HasRelEvent(unsigned int code) const {
  if (code > REL_MAX)
    return false;
  return EvdevBitIsSet(rel_bits_.data(), code);
}

bool EventDeviceInfo::HasAbsEvent(unsigned int code) const {
  if (code > ABS_MAX)
    return false;
  return EvdevBitIsSet(abs_bits_.data(), code);
}

bool EventDeviceInfo::HasMscEvent(unsigned int code) const {
  if (code > MSC_MAX)
    return false;
  return EvdevBitIsSet(msc_bits_.data(), code);
}

bool EventDeviceInfo::HasSwEvent(unsigned int code) const {
  if (code > SW_MAX)
    return false;
  return EvdevBitIsSet(sw_bits_.data(), code);
}

bool EventDeviceInfo::HasLedEvent(unsigned int code) const {
  if (code > LED_MAX)
    return false;
  return EvdevBitIsSet(led_bits_.data(), code);
}

bool EventDeviceInfo::HasFfEvent(unsigned int code) const {
  if (code > FF_MAX)
    return false;
  return EvdevBitIsSet(ff_bits_.data(), code);
}

bool EventDeviceInfo::HasProp(unsigned int code) const {
  if (code > INPUT_PROP_MAX)
    return false;
  return EvdevBitIsSet(prop_bits_.data(), code);
}

bool EventDeviceInfo::SupportsHeatmap() const {
  for (const auto& device_id : kHeatmapSupportedDevices) {
    if (input_id_.vendor == device_id.vendor &&
        input_id_.product == device_id.product_id) {
      return true;
    }
  }
  return false;
}

int32_t EventDeviceInfo::GetAbsMinimum(unsigned int code) const {
  return abs_info_[code].minimum;
}

int32_t EventDeviceInfo::GetAbsMaximum(unsigned int code) const {
  return abs_info_[code].maximum;
}

int32_t EventDeviceInfo::GetAbsResolution(unsigned int code) const {
  return abs_info_[code].resolution;
}

int32_t EventDeviceInfo::GetAbsValue(unsigned int code) const {
  return abs_info_[code].value;
}

input_absinfo EventDeviceInfo::GetAbsInfoByCode(unsigned int code) const {
  return abs_info_[code];
}

uint32_t EventDeviceInfo::GetAbsMtSlotCount() const {
  if (!HasAbsEvent(ABS_MT_SLOT))
    return 0;
  return GetAbsMaximum(ABS_MT_SLOT) + 1;
}

int32_t EventDeviceInfo::GetAbsMtSlotValue(unsigned int code,
                                           unsigned int slot) const {
  unsigned int index = code - EVDEV_ABS_MT_FIRST;
  DCHECK(index < EVDEV_ABS_MT_COUNT);
  return slot_values_[index][slot];
}

int32_t EventDeviceInfo::GetAbsMtSlotValueWithDefault(
    unsigned int code,
    unsigned int slot,
    int32_t default_value) const {
  if (!HasAbsEvent(code))
    return default_value;
  return GetAbsMtSlotValue(code, slot);
}

bool EventDeviceInfo::HasAbsXY() const {
  return HasAbsEvent(ABS_X) && HasAbsEvent(ABS_Y);
}

bool EventDeviceInfo::HasMTAbsXY() const {
  return HasAbsEvent(ABS_MT_POSITION_X) && HasAbsEvent(ABS_MT_POSITION_Y);
}

bool EventDeviceInfo::HasRelXY() const {
  return HasRelEvent(REL_X) && HasRelEvent(REL_Y);
}

bool EventDeviceInfo::HasMultitouch() const {
  return HasAbsEvent(ABS_MT_SLOT);
}

bool EventDeviceInfo::HasDirect() const {
  bool has_direct = HasProp(INPUT_PROP_DIRECT);
  bool has_pointer = HasProp(INPUT_PROP_POINTER);
  if (has_direct || has_pointer)
    return has_direct;

  switch (ProbeLegacyAbsoluteDevice()) {
    case LegacyAbsoluteDeviceType::TOUCHSCREEN:
      return true;

    case LegacyAbsoluteDeviceType::TABLET:
    case LegacyAbsoluteDeviceType::TOUCHPAD:
    case LegacyAbsoluteDeviceType::NONE:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool EventDeviceInfo::HasPointer() const {
  bool has_direct = HasProp(INPUT_PROP_DIRECT);
  bool has_pointer = HasProp(INPUT_PROP_POINTER);
  if (has_direct || has_pointer)
    return has_pointer;

  switch (ProbeLegacyAbsoluteDevice()) {
    case LegacyAbsoluteDeviceType::TOUCHPAD:
    case LegacyAbsoluteDeviceType::TABLET:
      return true;

    case LegacyAbsoluteDeviceType::TOUCHSCREEN:
    case LegacyAbsoluteDeviceType::NONE:
      return false;
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

bool EventDeviceInfo::HasStylus() const {
  return HasKeyEvent(BTN_TOOL_PEN) || HasKeyEvent(BTN_STYLUS) ||
         HasKeyEvent(BTN_STYLUS2);
}

bool EventDeviceInfo::IsSemiMultitouch() const {
  return HasProp(INPUT_PROP_SEMI_MT);
}

bool EventDeviceInfo::IsStylusButtonDevice() const {
  for (const auto& device_id : kStylusButtonDevices) {
    if (input_id_.vendor == device_id.vendor &&
        input_id_.product == device_id.product_id)
      return true;
  }

  return false;
}

bool EventDeviceInfo::IsMicrophoneMuteSwitchDevice() const {
  return HasSwEvent(SW_MUTE_DEVICE) && device_type_ == INPUT_DEVICE_INTERNAL;
}

bool EventDeviceInfo::UseLibinput() const {
  bool useLibinput = false;
  if (HasTouchpad()) {
    auto overridden_state =
        base::FeatureList::GetStateIfOverridden(ui::kLibinputHandleTouchpad);
    if (overridden_state.has_value()) {
      useLibinput = overridden_state.value();
    } else {
      useLibinput = !HasMultitouch() || !HasValidMTAbsXY() ||
                    IsSemiMultitouch() || IsForceLibinput(*this);
    }
  }

  return useLibinput;
}

void RecordBlocklistedKeyboardMetric(input_id input_id_) {
  static base::NoDestructor<base::flat_set<DeviceId>> logged_devices;
  auto [_, inserted] =
      logged_devices->insert({input_id_.vendor, input_id_.product});
  if (inserted) {
    base::UmaHistogramEnumeration(
        "ChromeOS.Inputs.ComboDeviceClassification",
        ComboDeviceClassification::kKnownKeyboardImposter);
  }
}

bool IsInKeyboardBlockList(input_id input_id_) {
  DeviceId id = {input_id_.vendor, input_id_.product};
  if (kKeyboardBlocklist.contains(id)) {
    RecordBlocklistedKeyboardMetric(input_id_);
    return true;
  }

  return false;
}

bool IsInKeyboardAllowList(input_id input_id_) {
  DeviceId id = {input_id_.vendor, input_id_.product};
  return kKeyboardAllowlist.contains(id);
}

bool EventDeviceInfo::HasKeyboard() const {
  KeyboardType type = GetKeyboardType();
  return type == KeyboardType::VALID_KEYBOARD ||
         type == KeyboardType::IN_ALLOWLIST;
}

KeyboardType EventDeviceInfo::GetKeyboardType() const {
  if (IsInKeyboardAllowList(input_id_)) {
    return KeyboardType::IN_ALLOWLIST;
  }
  if (!HasEventType(EV_KEY))
    return KeyboardType::NOT_KEYBOARD;
  if (IsInKeyboardBlockList(input_id_))
    return KeyboardType::IN_BLOCKLIST;
  if (IsStylusButtonDevice())
    return KeyboardType::STYLUS_BUTTON_DEVICE;

  // Check first 31 keys: If we have all of them, consider it a full
  // keyboard. This is exactly what udev does for ID_INPUT_KEYBOARD.
  for (int key = KEY_ESC; key <= KEY_D; ++key)
    if (!HasKeyEvent(key))
      return KeyboardType::NOT_KEYBOARD;

  return KeyboardType::VALID_KEYBOARD;
}

bool EventDeviceInfo::HasMouse() const {
  // The SteelSeries Stratus Duo claims to be a mouse over Bluetooth, preventing
  // it from being set up as a gamepad correctly, so check for its vendor and
  // product ID. (b/189491809)
  if (input_id_.vendor == kSteelSeriesBluetoothVendorId &&
      (input_id_.product == kSteelSeriesStratusDuoBluetoothProductId ||
      input_id_.product == kSteelSeriesStratusPlusBluetoothProductId)) {
    return false;
  }

  // When floss is enabled, it presents a virtual device used to wake the device
  // on bluetooth connection. This long term should be reduced down to not
  // appear as a mouse. For now, filter it out directly. (b/309017352)
  if (input_id_.vendor == kFlossVirtualSuspendVendorId &&
      input_id_.product == kFlossVirtualSuspendProductId &&
      name_ == kFlossVirtualSuspendName) {
    return false;
  }

  return HasRelXY() && !HasProp(INPUT_PROP_POINTING_STICK);
}

bool EventDeviceInfo::HasPointingStick() const {
  return HasRelXY() && HasProp(INPUT_PROP_POINTING_STICK);
}

bool EventDeviceInfo::HasTouchpad() const {
  return HasAbsXY() && HasPointer() && !HasStylus();
}

bool EventDeviceInfo::HasHapticTouchpad() const {
  return HasTouchpad() && HasProp(INPUT_PROP_HAPTICPAD);
}

bool EventDeviceInfo::HasTablet() const {
  return HasAbsXY() && HasPointer() && HasStylus();
}

bool EventDeviceInfo::HasTouchscreen() const {
  return HasAbsXY() && HasDirect();
}

bool EventDeviceInfo::HasStylusSwitch() const {
  return HasSwEvent(SW_PEN_INSERTED) && (device_type_ == INPUT_DEVICE_UNKNOWN ||
                                         device_type_ == INPUT_DEVICE_INTERNAL);
}

bool EventDeviceInfo::HasNumberpad() const {
  // Does not check for HasKeyboard(): the dynamic numberpad
  // and external standalone numeric-pads will not be considered
  // keyboards, if their descriptor happens to be correct.
  if (!HasEventType(EV_KEY))
    return false;

  // The block-lists for keyboards are useful; currently, if something is
  // falsely claiming to be a keyboard, it probably has false numberpad keys as
  // well. If a numberpad needs to be added to the keyboard block-list, then
  // consider whether we need an overriding allow-list here, or whether
  // it is time to grow the list into a more detailed structure that can
  // provides more specific information on what a device's capabilities are.
  if (IsInKeyboardBlockList(input_id_))
    return false;
  if (IsStylusButtonDevice())
    return false;
  // Internal USB devices that are keyboards tend to be hammer-likes
  // that we should not treat as numberpads.
  if (IsInternalUSB(input_id_))
    return false;

  // Consider a device to have a numberpad if it has all ten numeric keys.
  for (int key : {KEY_KP0, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP4, KEY_KP5, KEY_KP6,
                  KEY_KP7, KEY_KP8, KEY_KP9}) {
    if (!HasKeyEvent(key))
      return false;
  }
  return true;
}

bool EventDeviceInfo::HasGamepad() const {
  if (!HasEventType(EV_KEY))
    return false;

  // If the device has gamepad button, and it's not keyboard or tablet, it will
  // be considered to be a gamepad. Note: this WILL have false positives and
  // false negatives. A concrete solution will use ID_INPUT_JOYSTICK with some
  // patch removing false positives.
  bool support_gamepad_btn = false;
  for (int key = BTN_JOYSTICK; key <= BTN_THUMBR; ++key) {
    if (HasKeyEvent(key))
      support_gamepad_btn = true;
  }

  return support_gamepad_btn && !HasTablet() && !HasKeyboard();
}

bool EventDeviceInfo::HasValidMTAbsXY() const {
  const auto x = GetAbsInfoByCode(ABS_MT_POSITION_X);
  const auto y = GetAbsInfoByCode(ABS_MT_POSITION_Y);

  return x.resolution > 0 && y.resolution > 0;
}

bool EventDeviceInfo::SupportsRumble() const {
  return HasEventType(EV_FF) && HasFfEvent(FF_RUMBLE);
}

// static
ui::InputDeviceType EventDeviceInfo::GetInputDeviceTypeFromId(input_id id) {
  static constexpr struct {
    uint16_t vid;
    uint16_t pid;
  } kUSBInternalDevices[] = {
      {0x18d1, 0x502b},  // Google, Hammer PID (soraka)
      {0x18d1, 0x5030},  // Google, Whiskers PID (nocturne)
      {0x18d1, 0x503c},  // Google, Masterball PID (krane) // nocheck
      {0x18d1, 0x503d},  // Google, Magnemite PID (kodama)
      {0x18d1, 0x5044},  // Google, Moonball PID (kakadu)
      {0x18d1, 0x504c},  // Google, Zed PID (coachz)
      {0x18d1, 0x5050},  // Google, Don PID (katsu)
      {0x18d1, 0x5052},  // Google, Star PID (homestar)
      {0x18d1, 0x5056},  // Google, bland PID (mrbland)
      {0x18d1, 0x5057},  // Google, eel PID (wormdingler)
      {0x18d1, 0x505B},  // Google, Duck PID (quackingstick)
      {0x18d1, 0x5061},  // Google, Jewel PID (starmie)
      {0x18d1, 0x5067},  // Google, Spikyrock (wugtrio)
      {0x1fd2, 0x8103},  // LG, Internal TouchScreen PID
  };

  if (id.bustype == BUS_USB) {
    for (size_t i = 0; i < std::size(kUSBInternalDevices); ++i) {
      if (id.vendor == kUSBInternalDevices[i].vid &&
          id.product == kUSBInternalDevices[i].pid)
        return InputDeviceType::INPUT_DEVICE_INTERNAL;
    }
  }

  switch (id.bustype) {
    case BUS_I2C:
    case BUS_I8042:
      return ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
    case BUS_USB:
      return ui::InputDeviceType::INPUT_DEVICE_USB;
    case BUS_BLUETOOTH:
      return ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH;
    default:
      return ui::InputDeviceType::INPUT_DEVICE_UNKNOWN;
  }
}

// static
bool EventDeviceInfo::IsInternalUSB(input_id id) {
  return (id.bustype == BUS_USB && GetInputDeviceTypeFromId(id) ==
                                       InputDeviceType::INPUT_DEVICE_INTERNAL);
}

EventDeviceInfo::LegacyAbsoluteDeviceType
EventDeviceInfo::ProbeLegacyAbsoluteDevice() const {
  if (!HasAbsXY())
    return LegacyAbsoluteDeviceType::NONE;

  // Treat internal stylus devices as touchscreens.
  if (device_type_ == INPUT_DEVICE_INTERNAL && HasStylus())
    return LegacyAbsoluteDeviceType::TOUCHSCREEN;

  if (HasStylus())
    return LegacyAbsoluteDeviceType::TABLET;

  if (HasKeyEvent(BTN_TOOL_FINGER) && HasKeyEvent(BTN_TOUCH))
    return LegacyAbsoluteDeviceType::TOUCHPAD;

  if (HasKeyEvent(BTN_TOUCH))
    return LegacyAbsoluteDeviceType::TOUCHSCREEN;

  // ABS_Z mitigation for extra device on some Elo devices.
  if (HasKeyEvent(BTN_LEFT) && !HasAbsEvent(ABS_Z) &&
      !IsDenylistedAbsoluteMouseDevice(input_id_))
    return LegacyAbsoluteDeviceType::TOUCHSCREEN;

  return LegacyAbsoluteDeviceType::NONE;
}
std::ostream& operator<<(std::ostream& os, const EventDeviceType value) {
  switch (value) {
    case EventDeviceType::DT_KEYBOARD:
      return os << "ui::EventDeviceType::DT_KEYBOARD";
    case EventDeviceType::DT_MOUSE:
      return os << "ui::EventDeviceType::DT_MOUSE";
    case EventDeviceType::DT_POINTING_STICK:
      return os << "ui::EventDeviceType::DT_POINTING_STICK";
    case EventDeviceType::DT_TOUCHPAD:
      return os << "ui::EventDeviceType::DT_TOUCHPAD";
    case EventDeviceType::DT_TOUCHSCREEN:
      return os << "ui::EventDeviceType::DT_TOUCHSCREEN";
    case EventDeviceType::DT_MULTITOUCH:
      return os << "ui::EventDeviceType::DT_MULTITOUCH";
    case EventDeviceType::DT_MULTITOUCH_MOUSE:
      return os << "ui::EventDeviceType::DT_MULTITOUCH_MOUSE";
    case EventDeviceType::DT_ALL:
      return os << "ui::EventDeviceType::DT_ALL";
  }
  return os << "ui::EventDeviceType::unknown_value("
            << static_cast<unsigned int>(value) << ")";
}

std::ostream& operator<<(std::ostream& os, const KeyboardType value) {
  switch (value) {
    case KeyboardType::NOT_KEYBOARD:
      return os << "ui::KeyboardType::NOT_KEYBOARD";
    case KeyboardType::IN_BLOCKLIST:
      return os << "ui::KeyboardType::IN_BLOCKLIST";
    case KeyboardType::STYLUS_BUTTON_DEVICE:
      return os << "ui::KeyboardType::STYLUS_BUTTON_DEVICE";
    case KeyboardType::VALID_KEYBOARD:
      return os << "ui::KeyboardType::VALID_KEYBOARD";
    case KeyboardType::IN_ALLOWLIST:
      return os << "ui::KeyboardType::IN_ALLOWLIST";
  }
  return os << "ui::KeyboardType::unknown_value("
            << static_cast<unsigned int>(value) << ")";
}

}  // namespace ui
