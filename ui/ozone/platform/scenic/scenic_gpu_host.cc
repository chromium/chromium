// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_gpu_host.h"

#include <inttypes.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/ozone/public/mojom/scenic_gpu_host.mojom.h"
#include "ui/ozone/public/mojom/scenic_gpu_service.mojom.h"

namespace {

using BinderCallback =
    base::RepeatingCallback<void(const std::string&,
                                 mojo::ScopedMessagePipeHandle)>;

template <typename Interface>
void BindInterface(mojo::PendingReceiver<Interface> receiver,
                   const BinderCallback& binder_callback) {
  binder_callback.Run(Interface::Name_, receiver.PassPipe());
}

}  // namespace

namespace ui {

ScenicGpuHost::ScenicGpuHost(ScenicWindowManager* scenic_window_manager)
    : scenic_window_manager_(scenic_window_manager),
      ui_thread_runner_(base::ThreadTaskRunnerHandle::Get()) {
  DETACH_FROM_THREAD(io_thread_checker_);
}

ScenicGpuHost::~ScenicGpuHost() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
}

mojo::PendingRemote<mojom::ScenicGpuHost>
ScenicGpuHost::CreateHostProcessSelfRemote() {
  DCHECK(!host_receiver_.is_bound());
  return host_receiver_.BindNewPipeAndPassRemote();
}

void ScenicGpuHost::AttachSurfaceToWindow(
    int32_t window_id,
    mojo::ScopedHandle surface_view_holder_token_mojo) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  ScenicWindow* scenic_window = scenic_window_manager_->GetWindow(window_id);
  if (!scenic_window)
    return;
  fuchsia::ui::views::ViewHolderToken surface_view_holder_token;
  surface_view_holder_token.value = zx::eventpair(
      mojo::UnwrapPlatformHandle(std::move(surface_view_holder_token_mojo))
          .TakeHandle());
  scenic_window->AttachSurfaceView(std::move(surface_view_holder_token));
}

void ScenicGpuHost::OnGpuProcessLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    base::RepeatingCallback<void(IPC::Message*)> send_callback) {
  NOTREACHED();
}

void ScenicGpuHost::OnChannelDestroyed(int host_id) {}

void ScenicGpuHost::OnGpuServiceLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);

  mojo::PendingRemote<mojom::ScenicGpuService> scenic_gpu_service;
  BindInterface(scenic_gpu_service.InitWithNewPipeAndPassReceiver(), binder);
  ui_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ScenicGpuHost::OnGpuServiceLaunchedOnUI,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(scenic_gpu_service)));
}

void ScenicGpuHost::OnGpuServiceLaunchedOnUI(
    mojo::PendingRemote<mojom::ScenicGpuService> gpu_service) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  gpu_receiver_.reset();
  gpu_service_.reset();

  gpu_service_.Bind(std::move(gpu_service));
  gpu_service_->Initialize(gpu_receiver_.BindNewPipeAndPassRemote());
}

void ScenicGpuHost::OnMessageReceived(const IPC::Message& message) {
  NOTREACHED();
}

}  // namespace ui
