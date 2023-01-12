// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_device_connector.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ui/ozone/platform/drm/host/host_drm_device.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {

DrmDeviceConnector::DrmDeviceConnector(
    scoped_refptr<HostDrmDevice> host_drm_device)
    : host_drm_device_(host_drm_device) {}

DrmDeviceConnector::~DrmDeviceConnector() = default;

void DrmDeviceConnector::OnChannelDestroyed(int host_id) {
  if (host_id != host_id_)
    return;
  host_drm_device_->OnGpuServiceLost();
}

void DrmDeviceConnector::OnGpuServiceLaunched(
    int host_id,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  // We need to preserve |binder| to let us bind interfaces later.
  binder_callback_ = std::move(binder);
  host_id_ = host_id;

  mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device;
  BindInterfaceDrmDevice(&drm_device);

  // This method is called before ash::Shell::Init which breaks assumptions
  // since the displays won't be marked as dummy but we don't have the active
  // list yet from the GPU process.
  // TODO(rjkroege): simplify this code path once GpuProcessHost always lives
  // on the UI thread.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&HostDrmDevice::OnGpuServiceLaunched,
                                host_drm_device_, std::move(drm_device)));
}

void DrmDeviceConnector::BindInterfaceDrmDevice(
    mojo::PendingRemote<ui::ozone::mojom::DrmDevice>* drm_device) const {
  binder_callback_.Run(ui::ozone::mojom::DrmDevice::Name_,
                       drm_device->InitWithNewPipeAndPassReceiver().PassPipe());
}

void DrmDeviceConnector::ConnectSingleThreaded(
    mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device) {
  host_drm_device_->OnGpuServiceLaunched(std::move(drm_device));
}

}  // namespace ui
