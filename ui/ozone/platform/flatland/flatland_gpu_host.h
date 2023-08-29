// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_GPU_HOST_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_GPU_HOST_H_

#include <inttypes.h>

#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/ozone/platform/flatland/mojom/scenic_gpu_host.mojom.h"
#include "ui/ozone/platform/flatland/mojom/scenic_gpu_service.mojom.h"
#include "ui/ozone/public/gpu_platform_support_host.h"

namespace ui {
class FlatlandWindowManager;

// Browser process object which supports a GPU process host.
//
// Once a GPU process starts, this objects binds to it over mojo and
// enables exchange of Flatland resources with it. In particular, we must
// exchange export tokens for each view surface, so that the surface can
// present to Flatland views managed by the browser.
class FlatlandGpuHost : public mojom::ScenicGpuHost,
                        public GpuPlatformSupportHost {
 public:
  FlatlandGpuHost(FlatlandWindowManager* flatland_window_manager);
  ~FlatlandGpuHost() override;
  FlatlandGpuHost(const FlatlandGpuHost&) = delete;
  FlatlandGpuHost& operator=(const FlatlandGpuHost&) = delete;

  // Binds the receiver for the main process surface factory.
  void Initialize(mojo::PendingReceiver<mojom::ScenicGpuHost> pending_receiver);

  // Shuts down mojo service. After calling shutdown, it's safe to call
  // Initialize() again.
  void Shutdown();

  // mojom::ScenicGpuHost:
  void AttachSurfaceToWindow(
      int32_t window_id,
      mojo::PlatformHandle surface_view_holder_token_mojo) override;

  // GpuPlatformSupportHost:
  void OnChannelDestroyed(int host_id) override;
  void OnGpuServiceLaunched(
      int host_id,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override;

 private:
  void UpdateReceiver(uint32_t service_launch_count,
                      mojo::PendingReceiver<mojom::ScenicGpuHost> receiver);

  FlatlandWindowManager* const flatland_window_manager_;
  mojo::Receiver<mojom::ScenicGpuHost> host_receiver_{this};
  mojo::Receiver<mojom::ScenicGpuHost> gpu_receiver_{this};

  mojo::Remote<mojom::ScenicGpuService> gpu_service_;

  THREAD_CHECKER(ui_thread_checker_);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_GPU_HOST_H_
