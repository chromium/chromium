// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_
#define UI_GL_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_

#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/vsync_observer.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace gl {
class VSyncThreadWin;

class GL_EXPORT DirectCompositionChildSurfaceWin : public GLSurfaceEGL,
                                                   public VSyncObserver {
 public:
  using VSyncCallback =
      base::RepeatingCallback<void(base::TimeTicks, base::TimeDelta)>;
  DirectCompositionChildSurfaceWin(VSyncCallback vsync_callback,
                                   bool use_angle_texture_offset,
                                   size_t max_pending_frames,
                                   bool force_full_damage);

  // GLSurfaceEGL implementation.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  gfx::Size GetSize() override;
  bool IsOffscreen() override;
  void* GetHandle() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::SurfaceOrigin GetOrigin() const override;
  bool SupportsPostSubBuffer() override;
  bool OnMakeCurrent(GLContext* context) override;
  bool SupportsDCLayers() const override;
  bool SetDrawRectangle(const gfx::Rect& rect) override;
  gfx::Vector2d GetDrawOffset() const override;
  void SetVSyncEnabled(bool enabled) override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  bool SetEnableDCLayers(bool enable) override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  bool SupportsGpuVSync() const override;
  void SetGpuVSyncEnabled(bool enabled) override;

  // VSyncObserver implementation.
  void OnVSync(base::TimeTicks vsync_time, base::TimeDelta interval) override;

  static bool IsDirectCompositionSwapChainFailed();

  const Microsoft::WRL::ComPtr<IDCompositionSurface>& dcomp_surface() const {
    return dcomp_surface_;
  }

  const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }

  uint64_t dcomp_surface_serial() const { return dcomp_surface_serial_; }

  void SetDCompSurfaceForTesting(
      Microsoft::WRL::ComPtr<IDCompositionSurface> surface);

 protected:
  ~DirectCompositionChildSurfaceWin() override;

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

  void EnqueuePendingFrame(PresentationCallback callback);
  void CheckPendingFrames();

  void StartOrStopVSyncThread();

  bool VSyncCallbackEnabled() const;

  void HandleVSyncOnMainThread(base::TimeTicks vsync_time,
                               base::TimeDelta interval);

  // Release the texture that's currently being drawn to. If will_discard is
  // true then the surface should be discarded without swapping any contents
  // to it. Returns false if this fails.
  bool ReleaseDrawTexture(bool will_discard);

  gfx::Size size_ = gfx::Size(1, 1);
  bool enable_dc_layers_ = false;
  bool has_alpha_ = true;
  bool vsync_enabled_ = true;
  gfx::ColorSpace color_space_;

  // This is a placeholder surface used when not rendering to the
  // DirectComposition surface.
  EGLSurface default_surface_ = 0;

  // This is the real surface representing the backbuffer. It may be null
  // outside of a BeginDraw/EndDraw pair.
  EGLSurface real_surface_ = 0;
  bool first_swap_ = true;
  gfx::Rect swap_rect_;
  gfx::Vector2d draw_offset_;

  // This is a number that increments once for every EndDraw on a surface, and
  // is used to determine when the contents have changed so Commit() needs to
  // be called on the device.
  uint64_t dcomp_surface_serial_ = 0;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;
  Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> draw_texture_;

  const VSyncCallback vsync_callback_;
  const bool use_angle_texture_offset_;
  const size_t max_pending_frames_;
  const bool force_full_damage_;

  VSyncThreadWin* const vsync_thread_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  bool vsync_thread_started_ = false;
  bool vsync_callback_enabled_ GUARDED_BY(vsync_callback_enabled_lock_) = false;
  mutable base::Lock vsync_callback_enabled_lock_;

  // Queue of pending presentation callbacks.
  base::circular_deque<PendingFrame> pending_frames_;

  base::TimeTicks last_vsync_time_;
  base::TimeDelta last_vsync_interval_;

  base::WeakPtrFactory<DirectCompositionChildSurfaceWin> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DirectCompositionChildSurfaceWin);
};

}  // namespace gl

#endif  // UI_GL_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_
