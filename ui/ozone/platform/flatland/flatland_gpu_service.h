// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_GPU_SERVICE_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_GPU_SERVICE_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "ui/ozone/platform/flatland/mojom/scenic_gpu_host.mojom.h"
#include "ui/ozone/platform/flatland/mojom/scenic_gpu_service.mojom.h"

namespace ui {

// GPU process service that enables presentation to Flatland.
//
// This object exposes a mojo service to the browser process from the GPU
// process. The browser binds it to enable exchange of Flatland resources.
// In particular, we must exchange export tokens for each surface,
// so that surfaces can present to Flatland managed by the browser.
class FlatlandGpuService : public mojom::ScenicGpuService {
 public:
  FlatlandGpuService(
      mojo::PendingReceiver<mojom::ScenicGpuHost> gpu_host_receiver);
  ~FlatlandGpuService() override;
  FlatlandGpuService(const FlatlandGpuService&) = delete;
  FlatlandGpuService& operator=(const FlatlandGpuService&) = delete;

  base::RepeatingCallback<void(mojo::PendingReceiver<mojom::ScenicGpuService>)>
  GetBinderCallback();

  // mojom::ScenicGpuService:
  void Initialize(mojo::PendingRemote<mojom::ScenicGpuHost> gpu_host) override;

 private:
  void AddReceiver(mojo::PendingReceiver<mojom::ScenicGpuService> receiver);

  mojo::PendingReceiver<mojom::ScenicGpuHost> gpu_host_receiver_;

  mojo::ReceiverSet<mojom::ScenicGpuService> receiver_set_;

  base::WeakPtrFactory<FlatlandGpuService> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_GPU_SERVICE_H_
