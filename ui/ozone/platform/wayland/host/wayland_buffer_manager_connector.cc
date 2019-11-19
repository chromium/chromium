// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_connector.h"

#include "base/bind.h"
#include "base/task_runner_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"

namespace ui {

namespace {

// TODO(msisov): In the future when GpuProcessHost is moved to vizhost, remove
// this utility code.
using BinderCallback = ui::GpuPlatformSupportHost::GpuHostBindInterfaceCallback;

void BindInterfaceInGpuProcess(const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle interface_pipe,
                               const BinderCallback& binder_callback) {
  return binder_callback.Run(interface_name, std::move(interface_pipe));
}

template <typename Interface>
void BindInterfaceInGpuProcess(mojo::PendingReceiver<Interface> request,
                               const BinderCallback& binder_callback) {
  BindInterfaceInGpuProcess(Interface::Name_, std::move(request.PassPipe()),
                            binder_callback);
}

}  // namespace

WaylandBufferManagerConnector::WaylandBufferManagerConnector(
    WaylandBufferManagerHost* buffer_manager_host)
    : buffer_manager_host_(buffer_manager_host) {}

WaylandBufferManagerConnector::~WaylandBufferManagerConnector() = default;

void WaylandBufferManagerConnector::OnGpuProcessLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    base::RepeatingCallback<void(IPC::Message*)> send_callback) {}

void WaylandBufferManagerConnector::OnChannelDestroyed(int host_id) {
  buffer_manager_host_->OnChannelDestroyed();
}

void WaylandBufferManagerConnector::OnMessageReceived(
    const IPC::Message& message) {
  NOTREACHED() << "This class should only be used with mojo transport but here "
                  "we're wrongly getting invoked to handle IPC communication.";
}

void WaylandBufferManagerConnector::OnGpuServiceLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  terminate_callback_ = std::move(terminate_callback);
  binder_ = std::move(binder);

  io_runner_ = io_runner;
  auto on_terminate_gpu_cb =
      base::BindOnce(&WaylandBufferManagerConnector::OnTerminateGpuProcess,
                     base::Unretained(this));
  buffer_manager_host_->SetTerminateGpuCallback(std::move(on_terminate_gpu_cb));

  base::PostTaskAndReplyWithResult(
      ui_runner.get(), FROM_HERE,
      base::BindOnce(&WaylandBufferManagerHost::BindInterface,
                     base::Unretained(buffer_manager_host_)),
      base::BindOnce(
          &WaylandBufferManagerConnector::OnBufferManagerHostPtrBinded,
          base::Unretained(this)));
}

void WaylandBufferManagerConnector::OnBufferManagerHostPtrBinded(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
        buffer_manager_host) const {
  mojo::Remote<ozone::mojom::WaylandBufferManagerGpu> buffer_manager_gpu_remote;
  auto receiver = buffer_manager_gpu_remote.BindNewPipeAndPassReceiver();
  BindInterfaceInGpuProcess(std::move(receiver), binder_);
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
  io_runner_->PostTask(FROM_HERE, base::BindOnce(std::move(terminate_callback_),
                                                 std::move(message)));
}

}  // namespace ui
