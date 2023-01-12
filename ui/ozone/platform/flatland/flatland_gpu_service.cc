// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_gpu_service.h"

#include "base/functional/bind.h"
#include "mojo/public/c/system/message_pipe.h"

namespace ui {

FlatlandGpuService::FlatlandGpuService(
    mojo::PendingReceiver<mojom::ScenicGpuHost> gpu_host_receiver)
    : gpu_host_receiver_(std::move(gpu_host_receiver)) {}

FlatlandGpuService::~FlatlandGpuService() {}

base::RepeatingCallback<void(mojo::PendingReceiver<mojom::ScenicGpuService>)>
FlatlandGpuService::GetBinderCallback() {
  return base::BindRepeating(&FlatlandGpuService::AddReceiver,
                             weak_ptr_factory_.GetWeakPtr());
}

void FlatlandGpuService::Initialize(
    mojo::PendingRemote<mojom::ScenicGpuHost> gpu_host) {
  // The FlatlandGpuService acts as a bridge to bind the
  // Remote<mojom::ScenicGpuHost>, owned by FlatlandSurfaceFactory and
  // received in the constructor, and the
  // mojo::Receiver<mojom::ScenicGpuHost>, owned by FlatlandGpuHost and
  // received as a parameter in this function. Using mojo::FusePipes is the only
  // way to "bind" a pending remote with a pending receiver.
  bool result =
      mojo::FusePipes(std::move(gpu_host_receiver_), std::move(gpu_host));
  DCHECK(result);
}

void FlatlandGpuService::AddReceiver(
    mojo::PendingReceiver<mojom::ScenicGpuService> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

}  // namespace ui
