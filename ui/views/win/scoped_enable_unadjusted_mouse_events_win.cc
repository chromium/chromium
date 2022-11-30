// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/scoped_enable_unadjusted_mouse_events_win.h"

#include "base/logging.h"
#include "ui/views/win/hwnd_message_handler.h"

namespace views {
namespace {

// From the HID Usage Tables specification.
constexpr USHORT kGenericDesktopPage = 1;
constexpr USHORT kMouseUsage = 2;

std::unique_ptr<RAWINPUTDEVICE> GetRawInputDevices(HWND hwnd, DWORD flags) {
  std::unique_ptr<RAWINPUTDEVICE> device = std::make_unique<RAWINPUTDEVICE>();
  device->dwFlags = flags;
  device->usUsagePage = kGenericDesktopPage;
  device->usUsage = kMouseUsage;
  device->hwndTarget = hwnd;
  return device;
}

}  // namespace

ScopedEnableUnadjustedMouseEventsWin::ScopedEnableUnadjustedMouseEventsWin(
    HWNDMessageHandler* owner)
    : owner_(owner) {}

ScopedEnableUnadjustedMouseEventsWin::~ScopedEnableUnadjustedMouseEventsWin() {
  // Stop receiving raw input.
  std::unique_ptr<RAWINPUTDEVICE> device(
      GetRawInputDevices(nullptr, RIDEV_REMOVE));
  if (!RegisterRawInputDevices(device.get(), 1, sizeof(*device)))
    PLOG(INFO) << "RegisterRawInputDevices() failed for RIDEV_REMOVE ";

  DCHECK(owner_->using_wm_input());
  owner_->set_using_wm_input(false);
}

// static
std::unique_ptr<ScopedEnableUnadjustedMouseEventsWin>
ScopedEnableUnadjustedMouseEventsWin::StartMonitor(HWNDMessageHandler* owner) {
  std::unique_ptr<RAWINPUTDEVICE> device(
      GetRawInputDevices(owner->hwnd(), RIDEV_INPUTSINK));
  if (!RegisterRawInputDevices(device.get(), 1, sizeof(*device))) {
    PLOG(INFO) << "RegisterRawInputDevices() failed for RIDEV_INPUTSINK ";
    return nullptr;
  }
  DCHECK(!owner->using_wm_input());
  owner->set_using_wm_input(true);
  return std::make_unique<ScopedEnableUnadjustedMouseEventsWin>(owner);
}

}  // namespace views
