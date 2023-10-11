// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_connector.h"

#include <vector>

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

  // Bug fix ids are sent from the server one by one asynchronously. Wait for
  // all bug fix ids are ready before initializing WaylandBufferManagerGpu. If
  // bug fix ids are already ready, it immediately calls OnAllBugFixesSent
  // synchronously.
  // TODO(crbug.com/1487446): This always runs synchronously now due to the
  // temporal solution for avoiding the race condition. It may return empty bug
  // fix ids while there are ids sent from Ash.
  buffer_manager_host_->WaitForAllBugFixIds(
      base::BindOnce(&WaylandBufferManagerConnector::OnAllBugFixesSent,
                     weak_factory_.GetWeakPtr()));
}

void WaylandBufferManagerConnector::OnAllBugFixesSent(
    const std::vector<uint32_t>& bug_fix_ids) {
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
      buffer_manager_host_->SupportsSinglePixelBuffer(), bug_fix_ids);
}

void WaylandBufferManagerConnector::OnTerminateGpuProcess(std::string message) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  DCHECK(!terminate_callback_.is_null());
  std::move(terminate_callback_).Run(std::move(message));
}

}  // namespace ui
