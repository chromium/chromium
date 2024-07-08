// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/vr/wvr_device.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/public/cpp/features.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/android/view_android.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace wolvic {

namespace {

const std::vector<device::mojom::XRSessionFeature>& GetSupportedFeatures() {
  static base::NoDestructor<std::vector<device::mojom::XRSessionFeature>>
      kSupportedFeatures{
          {device::mojom::XRSessionFeature::REF_SPACE_VIEWER,
           device::mojom::XRSessionFeature::REF_SPACE_LOCAL,
           device::mojom::XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
           device::mojom::XRSessionFeature::REF_SPACE_BOUNDED_FLOOR,
           device::mojom::XRSessionFeature::REF_SPACE_UNBOUNDED,
           device::mojom::XRSessionFeature::ANCHORS}};

  return *kSupportedFeatures;
}

}  // namespace

WvrDevice::WvrDevice()
    : VRDeviceBase(device::mojom::XRDeviceId::WVR_DEVICE_ID),
      main_thread_task_runner_(
          base::SingleThreadTaskRunner::GetCurrentDefault()) {

  std::vector<device::mojom::XRSessionFeature> device_features(
      GetSupportedFeatures());

  // Only support hand input if the feature flag is enabled.
  if (base::FeatureList::IsEnabled(device::features::kWebXrHandInput)) {
    device_features.emplace_back(device::mojom::XRSessionFeature::HAND_INPUT);
  }

  SetSupportedFeatures(device_features);
  SetArBlendModeSupported(true);
}

WvrDevice::~WvrDevice() {
  if (HasExclusiveSession()) {
    // We potentially could be destroyed during a navigation before processing
    // the exclusive session connection error handler.  In this case, the
    // delegate thinks we are still presenting.
    StopPresenting(base::NullCallback());
  }

  if (pending_request_session_callback_) {
    std::move(pending_request_session_callback_).Run(nullptr);
  }

  wvr_thread_ = nullptr;
}

void WvrDevice::RequestSession(
    device::mojom::XRRuntimeSessionOptionsPtr options,
    device::mojom::XRRuntime::RequestSessionCallback callback) {
  DCHECK_EQ(options->mode, device::mojom::XRSessionMode::kImmersiveVr);

  // We can only process one request at a time.
  if (pending_request_session_callback_) {
    std::move(callback).Run(nullptr);
    return;
  }

  pending_request_session_callback_ = std::move(callback);

  wvr_thread_ =
      std::make_unique<WvrThread>(CreateMainThreadCallback(base::BindOnce(
          &WvrDevice::OnWvrThreadReady, GetWeakPtr(), std::move(options))));

  wvr_thread_->Start();
}

void WvrDevice::PauseTracking() {
  // TODO : Handle tracking.
  paused_ = true;
}

void WvrDevice::ResumeTracking() {
  // TODO : Handle tracking.
  paused_ = false;
}

void WvrDevice::ShutdownSession(
    device::mojom::XRRuntime::ShutdownSessionCallback on_completed) {
  StopPresenting(std::move(on_completed));
}

void WvrDevice::PostTaskToWvrThread(base::OnceClosure task) {
  DCHECK(IsOnMainThread());
  wvr_thread_->GetWvrManager()->GetWvrThreadTaskRunner()->PostTask(
      FROM_HERE, std::move(task));
}

bool WvrDevice::IsOnMainThread() {
  return main_thread_task_runner_->BelongsToCurrentThread();
}

void WvrDevice::OnWvrThreadReady(
    device::mojom::XRRuntimeSessionOptionsPtr options) {
  PostTaskToWvrThread(base::BindOnce(
      &WvrGraphicsDelegate::InitializeGl, wvr_thread_->GetWvrGraphics()->GetWeakPtr(),
      CreateMainThreadCallback(
          base::BindOnce(&WvrDevice::OnWvrGlInitializationComplete,
                         GetWeakPtr(), std::move(options)))));
}

void WvrDevice::OnWvrGlInitializationComplete(
    device::mojom::XRRuntimeSessionOptionsPtr options) {
  DCHECK(IsOnMainThread());
  PostTaskToWvrThread(base::BindOnce(
      &WvrManager::StartWebXRPresentation,
      wvr_thread_->GetWvrManager()->GetWeakPtr(), std::move(options),
      CreateMainThreadCallback(
          base::BindOnce(&WvrDevice::OnStartPresenting, GetWeakPtr())),
      CreateMainThreadCallback(base::BindOnce(
          &WvrDevice::OnStopPresenting, GetWeakPtr(), base::NullCallback()))));
}

void WvrDevice::OnStartPresenting(device::mojom::XRSessionPtr session) {
  DCHECK(IsOnMainThread());
  DCHECK(pending_request_session_callback_);

  // Set HasExclusiveSession status to true. This lasts until OnSessionEnded.
  VRDeviceBase::OnStartPresenting();

  DCHECK(!exclusive_controller_receiver_.is_bound());

  auto session_result = device::mojom::XRRuntimeSessionResult::New();
  session_result->controller =
      exclusive_controller_receiver_.BindNewPipeAndPassRemote();
  session_result->session = std::move(session);

  std::move(pending_request_session_callback_).Run(std::move(session_result));

  // Unretained is safe because the error handler won't be called after the
  // binding has been destroyed.
  exclusive_controller_receiver_.set_disconnect_handler(base::BindOnce(
      &WvrDevice::OnPresentingControllerMojoConnectionError, GetWeakPtr()));
}

// XRSessionController
void WvrDevice::SetFrameDataRestricted(bool restricted) {
  // Presentation sessions can not currently be restricted.
  DCHECK(false);
}

void WvrDevice::OnPresentingControllerMojoConnectionError() {
  StopPresenting(base::NullCallback());
}

void WvrDevice::StopPresenting(
    device::mojom::XRRuntime::ShutdownSessionCallback on_completed) {
  DCHECK(IsOnMainThread());
  PostTaskToWvrThread(base::BindOnce(
      &WvrManager::ExitWebXRPresentation,
      wvr_thread_->GetWvrManager()->GetWeakPtr(),
      CreateMainThreadCallback(base::BindOnce(&WvrDevice::OnStopPresenting,
                                              GetWeakPtr(),
                                              std::move(on_completed)))));
}

void WvrDevice::OnStopPresenting(
    device::mojom::XRRuntime::ShutdownSessionCallback on_completed) {
  DCHECK(IsOnMainThread());
  OnExitPresent();
  exclusive_controller_receiver_.reset();
  wvr_thread_ = nullptr;

  if (on_completed)
    std::move(on_completed).Run();
}

}  // namespace wolvic
