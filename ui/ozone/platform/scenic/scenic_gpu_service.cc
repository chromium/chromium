// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_gpu_service.h"

#include "base/bind.h"
#include "mojo/public/c/system/message_pipe.h"

namespace ui {

ScenicGpuService::ScenicGpuService(
    mojo::PendingReceiver<mojom::ScenicGpuHost> gpu_host_receiver)
    : gpu_host_receiver_(std::move(gpu_host_receiver)) {}

ScenicGpuService::~ScenicGpuService() {}

base::RepeatingCallback<void(mojo::PendingReceiver<mojom::ScenicGpuService>)>
ScenicGpuService::GetBinderCallback() {
  return base::BindRepeating(&ScenicGpuService::AddReceiver,
                             weak_ptr_factory_.GetWeakPtr());
}

void ScenicGpuService::Initialize(
    mojo::PendingRemote<mojom::ScenicGpuHost> gpu_host) {
  // The ScenicGpuService acts as a bridge to bind the
  // Remote<mojom::ScenicGpuHost>, owned by ScenicSurfaceFactory and
  // received in the constructor, and the mojo::Receiver<mojom::ScenicGpuHost>,
  // owned by ScenicGpuHost and received as a parameter in this function. Using
  // mojo::FusePipes is the only way to "bind" a pending remote with a
  // pending receiver.
  bool result =
      mojo::FusePipes(std::move(gpu_host_receiver_), std::move(gpu_host));
  DCHECK(result);
}

void ScenicGpuService::AddReceiver(
    mojo::PendingReceiver<mojom::ScenicGpuService> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

}  // namespace ui
