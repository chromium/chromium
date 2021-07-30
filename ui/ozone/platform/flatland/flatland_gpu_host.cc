// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_gpu_host.h"

#include <inttypes.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "ui/ozone/platform/flatland/flatland_window.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"
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

FlatlandGpuHost::FlatlandGpuHost(FlatlandWindowManager* flatland_window_manager)
    : flatland_window_manager_(flatland_window_manager) {
  DETACH_FROM_THREAD(io_thread_checker_);
}

FlatlandGpuHost::~FlatlandGpuHost() {
  Shutdown();
}

void FlatlandGpuHost::Initialize(
    mojo::PendingReceiver<mojom::ScenicGpuHost> host_receiver) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  DCHECK(!ui_thread_runner_);
  ui_thread_runner_ = base::ThreadTaskRunnerHandle::Get();
  DCHECK(ui_thread_runner_);

  host_receiver_.Bind(std::move(host_receiver));
}

void FlatlandGpuHost::Shutdown() {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  ui_thread_runner_ = nullptr;
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
  // TODO(crbug.com/1230150): Create ContentLinkToken and AttachSurfaceContent.
}

void FlatlandGpuHost::OnChannelDestroyed(int host_id) {}

void FlatlandGpuHost::OnGpuServiceLaunched(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
    scoped_refptr<base::SingleThreadTaskRunner> process_host_runner,
    GpuHostBindInterfaceCallback binder,
    GpuHostTerminateCallback terminate_callback) {
  mojo::PendingRemote<mojom::ScenicGpuService> flatland_gpu_service;
  BindInterface(flatland_gpu_service.InitWithNewPipeAndPassReceiver(), binder);
  if (ui_runner->BelongsToCurrentThread()) {
    OnGpuServiceLaunchedOnUI(std::move(flatland_gpu_service));
  } else {
    DCHECK_CALLED_ON_VALID_THREAD(io_thread_checker_);
    ui_thread_runner_->PostTask(
        FROM_HERE, base::BindOnce(&FlatlandGpuHost::OnGpuServiceLaunchedOnUI,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(flatland_gpu_service)));
  }
}

void FlatlandGpuHost::OnGpuServiceLaunchedOnUI(
    mojo::PendingRemote<mojom::ScenicGpuService> gpu_service) {
  DCHECK_CALLED_ON_VALID_THREAD(ui_thread_checker_);

  gpu_receiver_.reset();
  gpu_service_.reset();

  gpu_service_.Bind(std::move(gpu_service));
  gpu_service_->Initialize(gpu_receiver_.BindNewPipeAndPassRemote());
}

}  // namespace ui
