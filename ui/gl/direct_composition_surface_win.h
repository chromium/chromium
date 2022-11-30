// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DIRECT_COMPOSITION_SURFACE_WIN_H_
#define UI_GL_DIRECT_COMPOSITION_SURFACE_WIN_H_

#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include "base/containers/circular_deque.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gl/child_window_win.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"
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
class DirectCompositionChildSurfaceWin;

class GL_EXPORT DirectCompositionSurfaceWin : public GLSurfaceEGL,
                                              public VSyncObserver {
 public:
  using VSyncCallback =
      base::RepeatingCallback<void(base::TimeTicks, base::TimeDelta)>;
  using OverlayHDRInfoUpdateCallback = base::RepeatingClosure;

  struct Settings {
    bool disable_nv12_dynamic_textures = false;
    bool disable_vp_scaling = false;
    bool disable_vp_super_resolution = false;
    size_t max_pending_frames = 2;
    bool use_angle_texture_offset = false;
    bool no_downscaled_overlay_promotion = false;
  };

  DirectCompositionSurfaceWin(
      GLDisplayEGL* display,
      HWND parent_window,
      VSyncCallback vsync_callback,
      const DirectCompositionSurfaceWin::Settings& settings);

  DirectCompositionSurfaceWin(const DirectCompositionSurfaceWin&) = delete;
  DirectCompositionSurfaceWin& operator=(const DirectCompositionSurfaceWin&) =
      delete;

  // GLSurfaceEGL implementation.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  gfx::Size GetSize() override;
  bool IsOffscreen() override;
  void* GetHandle() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback,
                              FrameData data) override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback callback,
                                FrameData data) override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  void SetVSyncEnabled(bool enabled) override;
  bool SetEnableDCLayers(bool enable) override;
  gfx::SurfaceOrigin GetOrigin() const override;
  bool SupportsPostSubBuffer() override;
  bool OnMakeCurrent(GLContext* context) override;
  bool SupportsDCLayers() const override;
  bool SupportsProtectedVideo() const override;
  bool SetDrawRectangle(const gfx::Rect& rect) override;
  gfx::Vector2d GetDrawOffset() const override;
  bool SupportsGpuVSync() const override;
  void SetGpuVSyncEnabled(bool enabled) override;
  // This schedules an overlay plane to be displayed on the next SwapBuffers
  // or PostSubBuffer call. Overlay planes must be scheduled before every swap
  // to remain in the layer tree. This surface's backbuffer doesn't have to be
  // scheduled with ScheduleDCLayer, as it's automatically placed in the layer
  // tree at z-order 0.
  bool ScheduleDCLayer(
      std::unique_ptr<ui::DCRendererLayerParams> params) override;
  void SetFrameRate(float frame_rate) override;

  // VSyncObserver implementation.
  void OnVSync(base::TimeTicks vsync_time, base::TimeDelta interval) override;

  bool SupportsDelegatedInk() override;
  void SetDelegatedInkTrailStartPoint(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata) override;
  void InitDelegatedInkPointRendererReceiver(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
          pending_receiver) override;

  HWND window() const { return window_; }

  scoped_refptr<base::TaskRunner> GetWindowTaskRunnerForTesting();

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetLayerSwapChainForTesting(
      size_t index) const;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetBackbufferSwapChainForTesting()
      const;

  scoped_refptr<DirectCompositionChildSurfaceWin> GetRootSurfaceForTesting()
      const;

  void GetSwapChainVisualInfoForTesting(size_t index,
                                        gfx::Transform* transform,
                                        gfx::Point* offset,
                                        gfx::Rect* clip_rect) const;

  DCLayerTree* GetLayerTreeForTesting() { return layer_tree_.get(); }

 protected:
  ~DirectCompositionSurfaceWin() override;

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

  HWND window_ = nullptr;
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

  scoped_refptr<DirectCompositionChildSurfaceWin> root_surface_;
  std::unique_ptr<DCLayerTree> layer_tree_;

  base::WeakPtrFactory<DirectCompositionSurfaceWin> weak_factory_{this};
};

}  // namespace gl

#endif  // UI_GL_DIRECT_COMPOSITION_SURFACE_WIN_H_
