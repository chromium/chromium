// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DCOMP_PRESENTER_H_
#define UI_GL_DCOMP_PRESENTER_H_

#include <d3d11.h>
#include <dcomp.h>
#include <windows.h>
#include <wrl/client.h>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/frame_data.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/child_window_win.h"
#include "ui/gl/direct_composition_surface_win.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/presenter.h"
#include "ui/gl/vsync_observer.h"

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
class GL_EXPORT DCompPresenter : public Presenter, public VSyncObserver {
 public:
  using VSyncCallback =
      base::RepeatingCallback<void(base::TimeTicks, base::TimeDelta)>;
  using OverlayHDRInfoUpdateCallback = base::RepeatingClosure;

  DCompPresenter(GLDisplayEGL* display,
                 VSyncCallback vsync_callback,
                 const DirectCompositionSurfaceWin::Settings& settings);

  DCompPresenter(const DCompPresenter&) = delete;
  DCompPresenter& operator=(const DCompPresenter&) = delete;

  // GLSurfaceEGL implementation.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  bool SupportsDCLayers() const override;
  bool SupportsProtectedVideo() const override;
  bool SetDrawRectangle(const gfx::Rect& rect) override;
  bool SupportsGpuVSync() const override;
  void SetGpuVSyncEnabled(bool enabled) override;
  // This schedules an overlay plane to be displayed on the next SwapBuffers
  // or PostSubBuffer call. Overlay planes must be scheduled before every swap
  // to remain in the layer tree. This surface's backbuffer doesn't have to be
  // scheduled with ScheduleDCLayer, as it's automatically placed in the layer
  // tree at z-order 0.
  bool ScheduleDCLayer(std::unique_ptr<DCLayerOverlayParams> params) override;
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

  HWND window() const { return child_window_.window(); }

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

  bool VSyncCallbackEnabled() const;

  void HandleVSyncOnMainThread(base::TimeTicks vsync_time,
                               base::TimeDelta interval);

  ChildWindowWin child_window_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;

  const VSyncCallback vsync_callback_;

  const raw_ptr<VSyncThreadWin> vsync_thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  bool vsync_thread_started_ = false;
  bool vsync_callback_enabled_ GUARDED_BY(vsync_callback_enabled_lock_) = false;
  mutable base::Lock vsync_callback_enabled_lock_;

  // Queue of pending presentation callbacks.
  base::circular_deque<PendingFrame> pending_frames_;
  const size_t max_pending_frames_;

  base::TimeTicks last_vsync_time_;
  base::TimeDelta last_vsync_interval_;

  std::unique_ptr<DCLayerTree> layer_tree_;

  // Set in |SetDrawRectangle| and cleared in |SwapBuffers|. Used to determine
  // if a D3D query should be created for this frame, due to a non-empty draw
  // rectangle.
  bool create_query_this_frame_ = false;

  base::WeakPtrFactory<DCompPresenter> weak_factory_{this};
};

}  // namespace gl

#endif  // UI_GL_DCOMP_PRESENTER_H_
