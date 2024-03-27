// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_CONNECTOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_CONNECTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/ozone/platform/wayland/mojom/wayland_buffer_manager.mojom.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {

class WaylandBufferManagerHost;

// A connector class which instantiates a connection between
// WaylandBufferManagerGpu on the GPU side and the WaylandBufferManagerHost
// object on the browser process side.
class WaylandBufferManagerConnector : public GpuPlatformSupportHost {
 public:
  explicit WaylandBufferManagerConnector(
      WaylandBufferManagerHost* buffer_manager_host);

  WaylandBufferManagerConnector(const WaylandBufferManagerConnector&) = delete;
  WaylandBufferManagerConnector& operator=(
      const WaylandBufferManagerConnector&) = delete;

  ~WaylandBufferManagerConnector() override;

  // GpuPlatformSupportHost:
  void OnChannelDestroyed(int host_id) override;
  void OnGpuServiceLaunched(
      int host_id,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override;

 private:
  void OnTerminateGpuProcess(std::string message);

  // Non-owned pointer, which is used to bind a mojo pointer to the
  // WaylandBufferManagerHost.
  const raw_ptr<WaylandBufferManagerHost, LeakedDanglingUntriaged>
      buffer_manager_host_;

  GpuHostBindInterfaceCallback binder_;
  GpuHostTerminateCallback terminate_callback_;

  // Owned by the ui thread.
  int host_id_ = -1;

  THREAD_CHECKER(ui_thread_checker_);

  base::WeakPtrFactory<WaylandBufferManagerConnector> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_CONNECTOR_H_
