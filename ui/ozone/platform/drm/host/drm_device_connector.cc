// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_device_connector.h"

#include <utility>

#include "base/bind.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ui/ozone/platform/drm/host/host_drm_device.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {

DrmDeviceConnector::DrmDeviceConnector(
    scoped_refptr<HostDrmDevice> host_drm_device)
    : host_drm_device_(host_drm_device) {}

DrmDeviceConnector::~DrmDeviceConnector() = default;

void DrmDeviceConnector::OnGpuProcessLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    base::RepeatingCallback<void(IPC::Message*)> send_callback) {
  NOTREACHED();
}

void DrmDeviceConnector::OnChannelDestroyed(int host_id) {
  if (host_id != host_id_)
    return;
  host_drm_device_->OnGpuServiceLost();
}

void DrmDeviceConnector::OnGpuServiceLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  // We can get into this state if a new instance of GpuProcessHost is created
  // before the old one is destroyed.
  if (host_drm_device_->IsConnected())
    host_drm_device_->OnGpuServiceLost();

  // We need to preserve |binder| to let us bind interfaces later.
  binder_callback_ = std::move(binder);
  host_id_ = host_id;

  mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device;
  BindInterfaceDrmDevice(&drm_device);

  host_drm_device_->OnGpuServiceLaunchedOnIOThread(std::move(drm_device),
                                                   ui_runner);
}

void DrmDeviceConnector::OnMessageReceived(const IPC::Message& message) {
  NOTREACHED() << "This class should only be used with mojo transport but here "
                  "we're wrongly getting invoked to handle IPC communication.";
}

void DrmDeviceConnector::BindInterfaceDrmDevice(
    mojo::PendingRemote<ui::ozone::mojom::DrmDevice>* drm_device) const {
  binder_callback_.Run(ui::ozone::mojom::DrmDevice::Name_,
                       drm_device->InitWithNewPipeAndPassReceiver().PassPipe());
}

void DrmDeviceConnector::ConnectSingleThreaded(
    mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device) {
  host_drm_device_->OnGpuServiceLaunchedOnIOThread(
      std::move(drm_device), base::ThreadTaskRunnerHandle::Get());
}

}  // namespace ui
