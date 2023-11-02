// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/x11/device_list_cache_x11.h"

#include <algorithm>

#include "base/memory/singleton.h"
#include "ui/events/devices/x11/device_data_manager_x11.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"

namespace ui {

DeviceListCacheX11::DeviceListCacheX11() = default;

DeviceListCacheX11::~DeviceListCacheX11() = default;

DeviceListCacheX11* DeviceListCacheX11::GetInstance() {
  return base::Singleton<DeviceListCacheX11>::get();
}

void DeviceListCacheX11::UpdateDeviceList(x11::Connection* connection) {
  auto x_future = connection->xinput().ListInputDevices();
  auto xi_future = connection->xinput().XIQueryDevice();
  connection->Flush();
  if (auto reply = x_future.Sync())
    x_dev_list_ = reply->devices;
  if (auto reply = xi_future.Sync())
    xi_dev_list_ = reply->infos;
  updated_ = true;
}

const XDeviceList& DeviceListCacheX11::GetXDeviceList(
    x11::Connection* connection) {
  if (!updated_)
    UpdateDeviceList(connection);
  return x_dev_list_;
}

const XIDeviceList& DeviceListCacheX11::GetXI2DeviceList(
    x11::Connection* connection) {
  if (!updated_)
    UpdateDeviceList(connection);
  return xi_dev_list_;
}

}  // namespace ui
