// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_gpu_host.h"

#include <inttypes.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ui/ozone/platform/flatland/flatland_window.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"
#include "ui/ozone/platform/flatland/mojom/scenic_gpu_host.mojom.h"
#include "ui/ozone/platform/flatland/mojom/scenic_gpu_service.mojom.h"

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

FlatlandGpuHost::FlatlandGpuHost(FlatlandWindowManager* flatland_window_manager)
    : flatland_window_manager_(flatland_window_manager) {
}

FlatlandGpuHost::~FlatlandGpuHost() {
  Shutdown();
}

void FlatlandGpuHost::Initialize(
    mojo::PendingReceiver<mojom::ScenicGpuHost> host_receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  host_receiver_.Bind(std::move(host_receiver));
}

void FlatlandGpuHost::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  host_receiver_.reset();
  gpu_receiver_.reset();
  gpu_service_.reset();
}

void FlatlandGpuHost::AttachSurfaceToWindow(
    int32_t window_id,
    mojo::PlatformHandle surface_view_holder_token_mojo) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  FlatlandWindow* flatland_window =
      flatland_window_manager_->GetWindow(window_id);
  if (!flatland_window)
    return;
  fuchsia::ui::views::ViewportCreationToken surface_view_holder_token;
  surface_view_holder_token.value =
      zx::channel(surface_view_holder_token_mojo.TakeHandle());
  flatland_window->AttachSurfaceContent(std::move(surface_view_holder_token));
}

void FlatlandGpuHost::OnChannelDestroyed(int host_id) {}

void FlatlandGpuHost::OnGpuServiceLaunched(
    int host_id,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);
  mojo::PendingRemote<mojom::ScenicGpuService> flatland_gpu_service;
  BindInterface(flatland_gpu_service.InitWithNewPipeAndPassReceiver(), binder);

  gpu_receiver_.reset();
  gpu_service_.reset();

  gpu_service_.Bind(std::move(flatland_gpu_service));
  gpu_service_->Initialize(gpu_receiver_.BindNewPipeAndPassRemote());
}

}  // namespace ui
