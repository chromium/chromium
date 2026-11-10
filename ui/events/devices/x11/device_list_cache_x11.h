// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_X11_DEVICE_LIST_CACHE_X11_H_
#define UI_EVENTS_DEVICES_X11_DEVICE_LIST_CACHE_X11_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "ui/events/devices/x11/events_devices_x11_export.h"
#include "ui/gfx/x/xinput.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

using XDeviceList = std::vector<x11::Input::DeviceInfo>;
using XIDeviceList = std::vector<x11::Input::XIDeviceInfo>;

namespace ui {

// Caches the current XInput device list to minimize round-trip time to the X
// server whenever a device list is needed. Updated asynchronously on incoming
// XInput hierarchy and device-changed events.
class EVENTS_DEVICES_X11_EXPORT DeviceListCacheX11 {
 public:
  static DeviceListCacheX11* GetInstance();

  DeviceListCacheX11(const DeviceListCacheX11&) = delete;
  DeviceListCacheX11& operator=(const DeviceListCacheX11&) = delete;

  // Asynchronously fetches the XInput and XInput2 device lists from
  // `connection` and runs `callback` once both replies have arrived. If
  // another UpdateDeviceList() call occurs before an in-flight update
  // completes, callbacks are coalesced onto the latest request pair.
  void UpdateDeviceList(x11::Connection* connection,
                        base::OnceClosure callback = {});

  // Returns the list of devices associated with `connection`. Uses the old X11
  // protocol to get the list of the devices.
  const XDeviceList& GetXDeviceList(x11::Connection* connection);

  // Returns the list of devices associated with `connection`. Uses the newer
  // XINPUT2 protocol to get the list of devices. Before making this call, make
  // sure that XInput2 support is available (e.g. by calling
  // IsXInput2Available()).
  const XIDeviceList& GetXI2DeviceList(x11::Connection* connection);

 private:
  friend struct base::DefaultSingletonTraits<DeviceListCacheX11>;

  struct PendingUpdate;

  DeviceListCacheX11();
  ~DeviceListCacheX11();

  void SyncUpdateDeviceList(x11::Connection* connection);
  void OnListInputDevicesResponse(
      uint64_t serial,
      x11::Input::ListInputDevicesResponse response);
  void OnXIQueryDeviceResponse(uint64_t serial,
                               x11::Input::XIQueryDeviceResponse response);
  void MaybeFinishUpdate();

  bool updated_ = false;
  uint64_t next_serial_ = 0;
  std::unique_ptr<PendingUpdate> pending_update_;
  std::vector<base::OnceClosure> pending_callbacks_;
  XDeviceList x_dev_list_;
  XIDeviceList xi_dev_list_;
  base::WeakPtrFactory<DeviceListCacheX11> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_X11_DEVICE_LIST_CACHE_X11_H_
