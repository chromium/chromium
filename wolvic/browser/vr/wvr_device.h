// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_VR_WVR_DEVICE_H_
#define WOLVIC_BROWSER_VR_WVR_DEVICE_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom-forward.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"
#include "device/vr/vr_device_base.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "wolvic/browser/vr/wvr_thread.h"

namespace wolvic {

class WvrDevice : public device::VRDeviceBase,
                  public device::mojom::XRSessionController {
 public:
  WvrDevice();
  WvrDevice(const WvrDevice&) = delete;
  WvrDevice& operator=(const WvrDevice&) = delete;
  ~WvrDevice() override;

  // VRDeviceBase
  void RequestSession(
      device::mojom::XRRuntimeSessionOptionsPtr options,
      device::mojom::XRRuntime::RequestSessionCallback callback) override;
  void PauseTracking() override;
  void ResumeTracking() override;
  void ShutdownSession(
      device::mojom::XRRuntime::ShutdownSessionCallback) override;

 private:
  template <typename... Args>
  static void RunCallbackOnTaskRunner(
      const scoped_refptr<base::TaskRunner>& task_runner,
      base::OnceCallback<void(Args...)> callback,
      Args... args) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::forward<Args>(args)...));
  }
  template <typename... Args>
  base::OnceCallback<void(Args...)> CreateMainThreadCallback(
      base::OnceCallback<void(Args...)> callback) {
    return base::BindOnce(&WvrDevice::RunCallbackOnTaskRunner<Args...>,
                          main_thread_task_runner_, std::move(callback));
  }

  void PostTaskToWvrThread(base::OnceClosure task);
  bool IsOnMainThread();

  void OnWvrThreadReady(device::mojom::XRRuntimeSessionOptionsPtr options);
  void OnWvrGlInitializationComplete(
      device::mojom::XRRuntimeSessionOptionsPtr options);
  void OnStartPresenting(device::mojom::XRSessionPtr session);

  // XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  void OnPresentingControllerMojoConnectionError();
  void StopPresenting(
      device::mojom::XRRuntime::ShutdownSessionCallback on_completed);
  void OnStopPresenting(
      device::mojom::XRRuntime::ShutdownSessionCallback on_completed);

  base::WeakPtr<WvrDevice> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool paused_ = true;

  mojo::Receiver<device::mojom::XRSessionController>
      exclusive_controller_receiver_{this};

  device::mojom::XRRuntime::RequestSessionCallback
      pending_request_session_callback_;

  std::unique_ptr<WvrThread> wvr_thread_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  base::WeakPtrFactory<WvrDevice> weak_ptr_factory_{this};
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_VR_WVR_DEVICE_H_
