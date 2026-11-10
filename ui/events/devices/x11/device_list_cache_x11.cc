// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/x11/device_list_cache_x11.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/singleton.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/future.h"

namespace ui {

struct DeviceListCacheX11::PendingUpdate {
  PendingUpdate() = default;
  ~PendingUpdate() = default;

  uint64_t serial = 0;
  bool got_x = false;
  bool got_xi = false;
  std::optional<XDeviceList> x_dev_list;
  std::optional<XIDeviceList> xi_dev_list;
};

DeviceListCacheX11::DeviceListCacheX11() = default;

DeviceListCacheX11::~DeviceListCacheX11() = default;

DeviceListCacheX11* DeviceListCacheX11::GetInstance() {
  return base::Singleton<DeviceListCacheX11>::get();
}

void DeviceListCacheX11::UpdateDeviceList(x11::Connection* connection,
                                          base::OnceClosure callback) {
  if (callback) {
    pending_callbacks_.push_back(std::move(callback));
  }

  const uint64_t serial = ++next_serial_;
  auto update = std::make_unique<PendingUpdate>();
  update->serial = serial;
  pending_update_ = std::move(update);

  auto x_future = connection->xinput().ListInputDevices();
  auto xi_future = connection->xinput().XIQueryDevice();
  connection->Flush();

  x_future.OnResponse(
      base::BindOnce(&DeviceListCacheX11::OnListInputDevicesResponse,
                     weak_ptr_factory_.GetWeakPtr(), serial));
  xi_future.OnResponse(
      base::BindOnce(&DeviceListCacheX11::OnXIQueryDeviceResponse,
                     weak_ptr_factory_.GetWeakPtr(), serial));
}

const XDeviceList& DeviceListCacheX11::GetXDeviceList(
    x11::Connection* connection) {
  if (!updated_) {
    SyncUpdateDeviceList(connection);
  }
  return x_dev_list_;
}

const XIDeviceList& DeviceListCacheX11::GetXI2DeviceList(
    x11::Connection* connection) {
  if (!updated_) {
    SyncUpdateDeviceList(connection);
  }
  return xi_dev_list_;
}

void DeviceListCacheX11::SyncUpdateDeviceList(x11::Connection* connection) {
  auto x_future = connection->xinput().ListInputDevices();
  auto xi_future = connection->xinput().XIQueryDevice();
  connection->Flush();
  if (auto reply = x_future.Sync()) {
    x_dev_list_ = std::move(reply->devices);
  }
  if (auto reply = xi_future.Sync()) {
    xi_dev_list_ = std::move(reply->infos);
  }
  updated_ = true;
}

void DeviceListCacheX11::OnListInputDevicesResponse(
    uint64_t serial,
    x11::Input::ListInputDevicesResponse response) {
  if (!pending_update_ || pending_update_->serial != serial) {
    return;
  }
  pending_update_->got_x = true;
  if (response) {
    pending_update_->x_dev_list = std::move(response->devices);
  }
  MaybeFinishUpdate();
}

void DeviceListCacheX11::OnXIQueryDeviceResponse(
    uint64_t serial,
    x11::Input::XIQueryDeviceResponse response) {
  if (!pending_update_ || pending_update_->serial != serial) {
    return;
  }
  pending_update_->got_xi = true;
  if (response) {
    pending_update_->xi_dev_list = std::move(response->infos);
  }
  MaybeFinishUpdate();
}

void DeviceListCacheX11::MaybeFinishUpdate() {
  if (!pending_update_ || !pending_update_->got_x || !pending_update_->got_xi) {
    return;
  }

  auto update = std::move(pending_update_);
  if (update->x_dev_list.has_value()) {
    x_dev_list_ = std::move(*update->x_dev_list);
  }
  if (update->xi_dev_list.has_value()) {
    xi_dev_list_ = std::move(*update->xi_dev_list);
  }
  updated_ = true;

  auto callbacks = std::move(pending_callbacks_);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

}  // namespace ui
