// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_connector.h"

#include "base/bind.h"
#include "base/task_runner_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandBufferManagerConnector::WaylandBufferManagerConnector(
    WaylandBufferManagerHost* buffer_manager_host)
    : buffer_manager_host_(buffer_manager_host) {
  DETACH_FROM_THREAD(io_thread_checker_);
}

WaylandBufferManagerConnector::~WaylandBufferManagerConnector() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
}

void WaylandBufferManagerConnector::OnChannelDestroyed(int host_id) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  if (host_id_ == host_id)
    buffer_manager_host_->OnChannelDestroyed();
}

void WaylandBufferManagerConnector::OnGpuServiceLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  binder_ = std::move(binder);
  io_runner_ = io_runner;

  ui_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerConnector::OnGpuServiceLaunchedOnUI,
                     base::Unretained(this), host_id,
                     std::move(terminate_callback)));
}

void WaylandBufferManagerConnector::OnGpuServiceLaunchedOnUI(
    int host_id,
    GpuHostTerminateCallback terminate_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  host_id_ = host_id;

  auto on_terminate_gpu_cb =
      base::BindOnce(&WaylandBufferManagerConnector::OnTerminateGpuProcess,
                     base::Unretained(this));
  buffer_manager_host_->SetTerminateGpuCallback(std::move(on_terminate_gpu_cb));
  terminate_callback_ = std::move(terminate_callback);

  auto pending_remote = buffer_manager_host_->BindInterface();

  io_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WaylandBufferManagerConnector::OnBufferManagerHostPtrBinded,
          base::Unretained(this), std::move(pending_remote)));
}

void WaylandBufferManagerConnector::OnBufferManagerHostPtrBinded(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
        buffer_manager_host) const {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  mojo::Remote<ozone::mojom::WaylandBufferManagerGpu> buffer_manager_gpu_remote;
  binder_.Run(
      ozone::mojom::WaylandBufferManagerGpu::Name_,
      buffer_manager_gpu_remote.BindNewPipeAndPassReceiver().PassPipe());
  DCHECK(buffer_manager_gpu_remote);

  wl::BufferFormatsWithModifiersMap buffer_formats_with_modifiers =
      buffer_manager_host_->GetSupportedBufferFormats();
  bool supports_dma_buf = false;
#if defined(WAYLAND_GBM)
  supports_dma_buf = buffer_manager_host_->SupportsDmabuf();
#endif
  buffer_manager_gpu_remote->Initialize(std::move(buffer_manager_host),
                                        buffer_formats_with_modifiers,
                                        supports_dma_buf);
}

void WaylandBufferManagerConnector::OnTerminateGpuProcess(std::string message) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  DCHECK(!terminate_callback_.is_null());
  io_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(terminate_callback_),
                                                 std::move(message)));
}

}  // namespace ui
