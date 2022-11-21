// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_gpu_host.h"

#include <inttypes.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ui/ozone/platform/scenic/mojom/scenic_gpu_host.mojom.h"
#include "ui/ozone/platform/scenic/mojom/scenic_gpu_service.mojom.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"

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
    : scenic_window_manager_(scenic_window_manager) {
  DETACH_FROM_THREAD(io_thread_checker_);
}

ScenicGpuHost::~ScenicGpuHost() {
  Shutdown();
}

void ScenicGpuHost::Initialize(
    mojo::PendingReceiver<mojom::ScenicGpuHost> host_receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  DCHECK(!ui_thread_runner_);
  ui_thread_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
  DCHECK(ui_thread_runner_);

  host_receiver_.Bind(std::move(host_receiver));
}

void ScenicGpuHost::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  ui_thread_runner_ = nullptr;
  host_receiver_.reset();
  gpu_receiver_.reset();
  gpu_service_.reset();
}

void ScenicGpuHost::AttachSurfaceToWindow(
    int32_t window_id,
    mojo::PlatformHandle surface_view_holder_token_mojo) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  ScenicWindow* scenic_window = scenic_window_manager_->GetWindow(window_id);
  if (!scenic_window)
    return;
  fuchsia::ui::views::ViewHolderToken surface_view_holder_token;
  surface_view_holder_token.value =
      zx::eventpair(surface_view_holder_token_mojo.TakeHandle());
  scenic_window->AttachSurfaceView(std::move(surface_view_holder_token));
}

void ScenicGpuHost::OnChannelDestroyed(int host_id) {}

void ScenicGpuHost::OnGpuServiceLaunched(
    int host_id,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  mojo::PendingRemote<mojom::ScenicGpuService> scenic_gpu_service;
  BindInterface(scenic_gpu_service.InitWithNewPipeAndPassReceiver(), binder);

  gpu_receiver_.reset();
  gpu_service_.reset();

  gpu_service_.Bind(std::move(scenic_gpu_service));
  gpu_service_->Initialize(gpu_receiver_.BindNewPipeAndPassRemote());
}

}  // namespace ui
