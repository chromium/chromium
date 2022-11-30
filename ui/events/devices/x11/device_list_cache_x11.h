// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_X11_DEVICE_LIST_CACHE_X11_H_
#define UI_EVENTS_DEVICES_X11_DEVICE_LIST_CACHE_X11_H_

#include <map>
#include <memory>

#include "ui/events/devices/x11/events_devices_x11_export.h"
#include "ui/gfx/x/xinput.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

using XDeviceList = std::vector<x11::Input::DeviceInfo>;
using XIDeviceList = std::vector<x11::Input::XIDeviceInfo>;

namespace ui {

// A class to cache the current XInput device list. This minimized the
// round-trip time to the X server whenever such a device list is needed. The
// update function will be called on each incoming
// x11::Input::HierarchyEvent::opcode event.
class EVENTS_DEVICES_X11_EXPORT DeviceListCacheX11 {
 public:
  static DeviceListCacheX11* GetInstance();

  DeviceListCacheX11(const DeviceListCacheX11&) = delete;
  DeviceListCacheX11& operator=(const DeviceListCacheX11&) = delete;

  void UpdateDeviceList(x11::Connection* connection);

  // Returns the list of devices associated with |display|. Uses the old X11
  // protocol to get the list of the devices.
  const XDeviceList& GetXDeviceList(x11::Connection* connection);

  // Returns the list of devices associated with |display|. Uses the newer
  // XINPUT2 protocol to get the list of devices. Before making this call, make
  // sure that XInput2 support is available (e.g. by calling
  // IsXInput2Available()).
  const XIDeviceList& GetXI2DeviceList(x11::Connection* connection);

 private:
  friend struct base::DefaultSingletonTraits<DeviceListCacheX11>;

  DeviceListCacheX11();
  ~DeviceListCacheX11();

  bool updated_ = false;
  XDeviceList x_dev_list_;
  XIDeviceList xi_dev_list_;
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_X11_DEVICE_LIST_CACHE_X11_H_
