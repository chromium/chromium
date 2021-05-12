// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_CONNECTOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_CONNECTOR_H_

#include "ui/ozone/public/gpu_platform_support_host.h"

#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/ozone/public/mojom/wayland/wayland_buffer_manager.mojom.h"

namespace ui {

class WaylandBufferManagerHost;

// A connector class which instantiates a connection between
// WaylandBufferManagerGpu on the GPU side and the WaylandBufferManagerHost
// object on the browser process side.
class WaylandBufferManagerConnector : public GpuPlatformSupportHost {
 public:
  explicit WaylandBufferManagerConnector(
      WaylandBufferManagerHost* buffer_manager_host);
  ~WaylandBufferManagerConnector() override;

  // GpuPlatformSupportHost:
  void OnChannelDestroyed(int host_id) override;
  void OnGpuServiceLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> process_host_runner,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override;

 private:
  void OnGpuServiceLaunchedOnUI(int host_id,
                                GpuHostTerminateCallback terminate_callback);

  void OnBufferManagerHostPtrBinded(
      mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
          buffer_manager_host) const;

  void OnTerminateGpuProcess(std::string message);

  // Non-owned pointer, which is used to bind a mojo pointer to the
  // WaylandBufferManagerHost.
  WaylandBufferManagerHost* const buffer_manager_host_;

  GpuHostBindInterfaceCallback binder_;
  GpuHostTerminateCallback terminate_callback_;

  scoped_refptr<base::SingleThreadTaskRunner> process_host_runner_;

  // Owned by the ui thread.
  int host_id_ = -1;

  THREAD_CHECKER(ui_thread_checker_);
  THREAD_CHECKER(process_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerConnector);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_CONNECTOR_H_
