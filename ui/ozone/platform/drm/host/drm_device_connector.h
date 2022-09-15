// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_CONNECTOR_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_CONNECTOR_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/ozone/platform/drm/mojom/device_cursor.mojom.h"
#include "ui/ozone/platform/drm/mojom/drm_device.mojom.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {
class HostDrmDevice;

// DrmDeviceConnector sets up mojo pipes connecting the Viz host to the DRM
// service.
class DrmDeviceConnector : public GpuPlatformSupportHost {
 public:
  explicit DrmDeviceConnector(scoped_refptr<HostDrmDevice> host_drm_device);

  DrmDeviceConnector(const DrmDeviceConnector&) = delete;
  DrmDeviceConnector& operator=(const DrmDeviceConnector&) = delete;

  ~DrmDeviceConnector() override;

  // GpuPlatformSupportHost:
  void OnChannelDestroyed(int host_id) override;
  void OnGpuServiceLaunched(
      int host_id,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override;

  // BindInterfaceDrmDevice arranges for the drm_device to be connected.
  void BindInterfaceDrmDevice(
      mojo::PendingRemote<ui::ozone::mojom::DrmDevice>* drm_device) const;

  // Called in the single-threaded mode instead of OnGpuServiceLaunched() to
  // establish the connection.
  void ConnectSingleThreaded(
      mojo::PendingRemote<ui::ozone::mojom::DrmDevice> drm_device);

 private:
  // This will be used if we are operating under content/gpu without a service
  // manager.
  GpuHostBindInterfaceCallback binder_callback_;

  // The host_id from the last call to OnGpuServiceLaunched.
  int host_id_ = 0;

  const scoped_refptr<HostDrmDevice> host_drm_device_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_CONNECTOR_H_
