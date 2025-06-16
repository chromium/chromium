// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DCOMP_PRESENTER_H_
#define UI_GL_DCOMP_PRESENTER_H_

#include <windows.h>

#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include "base/containers/circular_deque.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/overlay_layer_id.h"
#include "ui/gl/child_window_win.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/presenter.h"
#include "ui/gl/vsync_thread_win.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gfx {
namespace mojom {
class DelegatedInkPointRenderer;
}  // namespace mojom
class DelegatedInkMetadata;
}  // namespace gfx

namespace gl {
class VSyncThreadWin;
class DCLayerTree;

// This class owns the DComp layer tree and its presentation. It does not own
// the root surface.
class GL_EXPORT DCompPresenter : public Presenter,
                                 public VSyncThreadWin::VSyncObserver {
 public:
  struct Settings {
    bool disable_nv12_dynamic_textures = false;
    bool disable_vp_auto_hdr = false;
    bool disable_vp_scaling = false;
    bool disable_vp_super_resolution = false;
    bool disable_dc_letterbox_video_optimization = false;
    bool force_dcomp_triple_buffer_video_swap_chain = false;
    bool no_downscaled_overlay_promotion = false;
  };

  explicit DCompPresenter(const Settings& settings);

  DCompPresenter(const DCompPresenter&) = delete;
  DCompPresenter& operator=(const DCompPresenter&) = delete;

  gfx::VSyncProvider* GetVSyncProvider();

  // Presenter implementation.
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  bool SupportsViewporter() const override;
  // This schedules overlay planes to be displayed on the next `Present` call.
  // An overlay plane must be scheduled before every `Present` to remain in the
  // layer tree. The primary plane should be included in `overlays`.
  void ScheduleDCLayers(std::vector<DCLayerOverlayParams> overlays) override;
  bool DestroyDCLayerTree() override;

  void Present(SwapCompletionCallback completion_callback,
               PresentationCallback presentation_callback,
               gfx::FrameData data) override;

  // VSyncObserver implementation.
  void OnVSync(base::TimeTicks vsync_time, base::TimeDelta interval) override;

  bool SupportsDelegatedInk() override;
  void SetDelegatedInkTrailStartPoint(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) override;
  void InitDelegatedInkPointRendererReceiver(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver) override;

  HWND GetWindow() const override;

  scoped_refptr<base::TaskRunner> GetWindowTaskRunnerForTesting();

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetLayerSwapChainForTesting(
      const gfx::OverlayLayerId& layer_id) const;

  void GetSwapChainVisualInfoForTesting(const gfx::OverlayLayerId& layer_id,
                                        gfx::Transform* out_transform,
                                        gfx::Point* out_offset,
                                        gfx::Rect* out_clip_rect) const;

  DCLayerTree* GetLayerTreeForTesting() { return layer_tree_.get(); }

 protected:
  ~DCompPresenter() override;

 private:
  struct PendingFrame {
    PendingFrame(PresentationCallback callback);
    PendingFrame(PendingFrame&& other);
    ~PendingFrame();
    PendingFrame& operator=(PendingFrame&& other);

    // Presentation callback enqueued in SwapBuffers().
    PresentationCallback callback;
  };

  void EnqueuePendingFrame(PresentationCallback callback);
  void CheckPendingFrames();

  void StartOrStopVSyncThread();

  void HandleVSyncOnMainThread(base::TimeTicks vsync_time,
                               base::TimeDelta interval);

  ChildWindowWin child_window_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  bool observing_vsync_ = false;

  // Queue of pending presentation callbacks.
  base::circular_deque<PendingFrame> pending_frames_;

  std::vector<DCLayerOverlayParams> pending_overlays_;

  base::TimeTicks last_vsync_time_;
  base::TimeDelta last_vsync_interval_;

  std::unique_ptr<DCLayerTree> layer_tree_;

  // Set in the ctor. Indicates whether vsync is enabled for the process.
  bool use_gpu_vsync_ = false;

  base::WeakPtrFactory<DCompPresenter> weak_factory_{this};
};

}  // namespace gl

#endif  // UI_GL_DCOMP_PRESENTER_H_
