// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_connector.h"

#include "base/functional/bind.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

WaylandBufferManagerConnector::WaylandBufferManagerConnector(
    WaylandBufferManagerHost* buffer_manager_host)
    : buffer_manager_host_(buffer_manager_host) {
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
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  binder_ = std::move(binder);
  host_id_ = host_id;

  auto on_terminate_gpu_cb =
      base::BindOnce(&WaylandBufferManagerConnector::OnTerminateGpuProcess,
                     weak_factory_.GetWeakPtr());
  buffer_manager_host_->SetTerminateGpuCallback(std::move(on_terminate_gpu_cb));
  terminate_callback_ = std::move(terminate_callback);

  auto pending_remote = buffer_manager_host_->BindInterface();

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
  buffer_manager_gpu_remote->Initialize(
      std::move(pending_remote), buffer_formats_with_modifiers,
      supports_dma_buf, buffer_manager_host_->SupportsViewporter(),
      buffer_manager_host_->SupportsAcquireFence(),
      buffer_manager_host_->SupportsOverlays(),
      buffer_manager_host_->GetSurfaceAugmentorVersion(),
      buffer_manager_host_->SupportsSinglePixelBuffer(),
      buffer_manager_host_->GetServerVersion());
}

void WaylandBufferManagerConnector::OnTerminateGpuProcess(std::string message) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  DCHECK(!terminate_callback_.is_null());
  std::move(terminate_callback_).Run(std::move(message));
}

}  // namespace ui
