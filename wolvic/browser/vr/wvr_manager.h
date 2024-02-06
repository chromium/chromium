// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_VR_WVR_MANAGER_H_
#define WOLVIC_BROWSER_VR_WVR_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "wolvic/browser/vr/moz_external_vr.h"
#include "wolvic/browser/vr/wvr_graphics_delegate.h"

namespace device {
class MailboxToSurfaceBridge;
}

namespace wolvic {

class WvrApi;

class WvrManager : public device::mojom::XRPresentationProvider,
                   public device::mojom::XRFrameDataProvider {
 public:
  WvrManager(WvrApi* wvr_api, WvrGraphicsDelegate* graphics);

  WvrManager(const WvrManager&) = delete;
  WvrManager& operator=(const WvrManager&) = delete;

  ~WvrManager() override;

  device::WebXrPresentationState* webxr() { return &webxr_; }

  // XRFrameDataProvider
  void GetFrameData(device::mojom::XRFrameDataRequestOptionsPtr options,
                    device::mojom::XRFrameDataProvider::GetFrameDataCallback
                        callback) override;
  void GetEnvironmentIntegrationProvider(
      mojo::PendingAssociatedReceiver<
          device::mojom::XREnvironmentIntegrationProvider> environment_provider)
      override;

  // XRPresentationProvider
  void SubmitFrameMissing(int16_t frame_index, const gpu::SyncToken&) override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox,
                   base::TimeDelta time_waited) override;
  void SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                   const gpu::SyncToken&,
                                   base::TimeDelta time_waited) override;
  void UpdateLayerBounds(int16_t frame_index,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;

  const scoped_refptr<base::SingleThreadTaskRunner>& GetWvrThreadTaskRunner() {
    return task_runner_;
  }

  base::WeakPtr<WvrManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void StartWebXRPresentation(
      device::mojom::XRRuntimeSessionOptionsPtr options,
      base::OnceCallback<void(device::mojom::XRSessionPtr)> callback,
      base::OnceClosure exit_callback);
  void ExitWebXRPresentation(base::OnceClosure callback);

 private:
  bool IsOnWvrThread() const;

  void ConnectPresentingService(
      device::mojom::XRRuntimeSessionOptionsPtr options,
      base::OnceCallback<void(device::mojom::XRSessionPtr)> callback);

  void DrawFrameSubmitNow(device::WebXrFrame* processing_frame);

  device::mojom::XRPresentationTransportOptionsPtr
  GetWebXrFrameTransportOptions(
      const device::mojom::XRRuntimeSessionOptionsPtr&);
  void CreateOrResizeWebXrSurface(const gfx::Size& size);
  void OnGpuProcessConnectionReady();
  void CreateSurfaceBridge(gl::SurfaceTexture* surface_texture);

  void OnWebXrFrameAvailable();

  std::vector<device::mojom::XRInputSourceStatePtr> GetInputSourceState();

  // Checks if we're in a valid state for starting animation of a new frame.
  // Invalid states include a previous animating frame that's not complete
  // yet (including deferred processing not having started yet), or timing
  // heuristics indicating that it should be retried later.
  bool WebVrCanAnimateFrame();
  // Call this after state changes that could result in WebVrCanAnimateFrame
  // becoming true.
  void WebXrTryStartAnimatingFrame();

  // Shared logic for SubmitFrame variants, including sanity checks.
  // Returns true if OK to proceed.
  bool SubmitFrameCommon(int16_t frame_index, base::TimeDelta time_waited);
  bool IsSubmitFrameExpected(int16_t frame_index);
  bool SubmitFrameInternal(int16_t frame_index);

  // Transition a frame from animating to processing.
  void ProcessWebVrFrameFromMailbox(int16_t frame_index,
                                    const gpu::MailboxHolder& mailbox);

  void ClosePresentationBindings();
  void OnSubmitClientMojoConnectionError();

  device::mojom::XRFrameDataProvider::GetFrameDataCallback
      get_frame_data_callback_;

  gfx::Transform floor_transform_;
  uint32_t stage_parameters_id_ = 0;

  // Communicate with the renderer.
  mojo::Receiver<device::mojom::XRPresentationProvider> presentation_receiver_{
      this};
  mojo::Receiver<device::mojom::XRFrameDataProvider> frame_data_receiver_{this};
  mojo::Remote<device::mojom::XRPresentationClient> submit_client_;
  base::queue<uint16_t> pending_frames_;

  raw_ptr<WvrApi> wvr_api_;
  raw_ptr<WvrGraphicsDelegate> graphics_;

  // on WVR Thread
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  device::WebXrPresentationState webxr_;
  std::unique_ptr<device::MailboxToSurfaceBridge> mailbox_bridge_;

  base::OnceClosure exit_vr_callback_;

  mozilla::gfx::VRControllerState
      controller_state_[mozilla::gfx::kVRControllerMaxCount];

  base::WeakPtrFactory<WvrManager> weak_ptr_factory_{this};
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_VR_WVR_MANAGER_H_
