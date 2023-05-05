// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_VR_WVR_MANAGER_H_
#define WOLVIC_BROWSER_VR_WVR_MANAGER_H_

#include "base/cancelable_callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webxr/mailbox_to_surface_bridge_impl.h"
#include "device/vr/android/web_xr_presentation_state.h"
#include "device/vr/public/cpp/xr_frame_sink_client.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/surface_handle.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_helper.h"
#include "wolvic/browser/vr/moz_external_vr.h"

namespace gl {
class GLSurface;
class SurfaceTexture;
}  // namespace gl

namespace wolvic {

class WvrManager : public device::mojom::XRPresentationProvider,
                   public device::mojom::XRFrameDataProvider {
 public:
  WvrManager();

  WvrManager(const WvrManager&) = delete;
  WvrManager& operator=(const WvrManager&) = delete;

  ~WvrManager() override;

  void InitializeGl(const gfx::Size& frame_size,
                    base::OnceClosure callback);

  // XRFrameDataProvider
  void GetFrameData(device::mojom::XRFrameDataRequestOptionsPtr options,
                    device::mojom::XRFrameDataProvider::GetFrameDataCallback
                        callback) override;
  void GetEnvironmentIntegrationProvider(
      mojo::PendingAssociatedReceiver<
          device::mojom::XREnvironmentIntegrationProvider> environment_provider)
      override;
  void SetInputSourceButtonListener(
      mojo::PendingAssociatedRemote<device::mojom::XRInputSourceButtonListener>)
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
      base::OnceCallback<void(device::mojom::XRSessionPtr)> callback);
  void ExitWebXRPresentation(base::OnceClosure callback);

 private:
  bool IsOnWvrThread() const;

  void ConnectPresentingService(
      device::mojom::XRRuntimeSessionOptionsPtr options,
      base::OnceCallback<void(device::mojom::XRSessionPtr)> callback);
  void CreateOrResizeWebXrSurface(const gfx::Size& size);
  void OnGpuProcessConnectionReady();
  void CreateSurfaceBridge();

  void OnWebXrFrameAvailable();
  void OnWebXrTimedOut();

  std::vector<device::mojom::XRInputSourceStatePtr> GetInputSourceState();

  bool CanStartNewAnimatingFrame();
  void TryStartAnimatingFrame();
  bool IsSubmitFrameExpected(int16_t frame_index);
  bool SubmitFrameInternal(int16_t frame_index);
  void ProcessFrameFromMailbox(int16_t frame_index,
                               const gpu::MailboxHolder& mailbox);

  void ClosePresentationBindings();
  void OnSubmitClientMojoConnectionError();

  void PushState(bool notifyCond = false);
  void PullState(const std::function<bool()>& waitCondition = {});

  base::TimeTicks pending_time_;
  device::mojom::XRFrameDataProvider::GetFrameDataCallback
      get_frame_data_callback_;

  // samplerExternalOES texture data for WebVR content image.
  GLuint texture_id_ = 0;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;

  scoped_refptr<gl::SurfaceTexture> surface_texture_;
  gfx::Size surface_size_ = {0, 0};
  gfx::Size screen_size_ = {0, 0};
  uint64_t last_frame_index_ = 0;

  // Java WVRSurfaceTexture instance.
  base::android::ScopedJavaGlobalRef<jobject> j_surface_texture_;

  // Communicate with the renderer.
  mojo::Receiver<device::mojom::XRPresentationProvider> presentation_receiver_{
      this};
  mojo::Receiver<device::mojom::XRFrameDataProvider> frame_data_receiver_{this};
  mojo::Remote<device::mojom::XRPresentationClient> submit_client_;

  // Communicate via mozilla shared memory.
  mozilla::gfx::VRBrowserState browser_state_;
  mozilla::gfx::VRSystemState system_state_;
  mozilla::gfx::VRExternalShmem* shmem_;

  // on WVR Thread
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  std::unique_ptr<device::WebXrPresentationState> webxr_;
  std::unique_ptr<device::MailboxToSurfaceBridge> mailbox_bridge_;

  base::CancelableOnceClosure webxr_frame_timeout_closure_;

  base::WeakPtrFactory<WvrManager> weak_ptr_factory_{this};
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_VR_WVR_MANAGER_H_
