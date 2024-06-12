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
    bool force_dcomp_triple_buffer_video_swap_chain = false;
    bool no_downscaled_overlay_promotion = false;
  };

  explicit DCompPresenter(const Settings& settings);

  DCompPresenter(const DCompPresenter&) = delete;
  DCompPresenter& operator=(const DCompPresenter&) = delete;

  void Destroy();
  gfx::VSyncProvider* GetVSyncProvider();

  // Presenter implementation.
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  bool SupportsViewporter() const override;
  // This schedules an overlay plane to be displayed on the next SwapBuffers
  // or PostSubBuffer call. Overlay planes must be scheduled before every swap
  // to remain in the layer tree. This surface's backbuffer doesn't have to be
  // scheduled with ScheduleDCLayer, as it's automatically placed in the layer
  // tree at z-order 0.
  void ScheduleDCLayer(std::unique_ptr<DCLayerOverlayParams> params) override;
  void SetFrameRate(float frame_rate) override;

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
      size_t index) const;

  void GetSwapChainVisualInfoForTesting(size_t index,
                                        gfx::Transform* transform,
                                        gfx::Point* offset,
                                        gfx::Rect* clip_rect) const;

  DCLayerTree* GetLayerTreeForTesting() { return layer_tree_.get(); }

 protected:
  ~DCompPresenter() override;

 private:
  struct PendingFrame {
    PendingFrame(Microsoft::WRL::ComPtr<ID3D11Query> query,
                 PresentationCallback callback);
    PendingFrame(PendingFrame&& other);
    ~PendingFrame();
    PendingFrame& operator=(PendingFrame&& other);

    // Event query issued after frame is presented.
    Microsoft::WRL::ComPtr<ID3D11Query> query;

    // Presentation callback enqueued in SwapBuffers().
    PresentationCallback callback;
  };

  void EnqueuePendingFrame(PresentationCallback callback, bool create_query);
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

  std::vector<std::unique_ptr<DCLayerOverlayParams>> pending_overlays_;

  base::TimeTicks last_vsync_time_;
  base::TimeDelta last_vsync_interval_;

  std::unique_ptr<DCLayerTree> layer_tree_;

  // Set in |SetDrawRectangle| and cleared in |SwapBuffers|. Used to determine
  // if a D3D query should be created for this frame, due to a non-empty draw
  // rectangle.
  bool create_query_this_frame_ = false;

  // Set in the ctor. Indicates whether vsync is enabled for the process.
  bool use_gpu_vsync_ = false;

  base::WeakPtrFactory<DCompPresenter> weak_factory_{this};
};

}  // namespace gl

#endif  // UI_GL_DCOMP_PRESENTER_H_
