// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DIRECT_COMPOSITION_SURFACE_WIN_H_
#define UI_GL_DIRECT_COMPOSITION_SURFACE_WIN_H_

#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include "base/callback.h"
#include "base/time/time.h"
#include "ui/gl/child_window_win.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gpu_switching_observer.h"
#include "ui/gl/vsync_observer.h"

namespace gl {
class DCLayerTree;
class DirectCompositionChildSurfaceWin;

class GL_EXPORT DirectCompositionSurfaceWin : public GLSurfaceEGL,
                                              public ui::GpuSwitchingObserver {
 public:
  using VSyncCallback =
      base::RepeatingCallback<void(base::TimeTicks, base::TimeDelta)>;
  using OverlayHDRInfoUpdateCallback = base::RepeatingClosure;

  struct Settings {
    bool disable_nv12_dynamic_textures = false;
    bool disable_larger_than_screen_overlays = false;
    bool disable_vp_scaling = false;
    size_t max_pending_frames = 2;
    bool use_angle_texture_offset = false;
    bool reset_vp_when_colorspace_changes = false;
    bool force_root_surface_full_damage = false;
  };

  DirectCompositionSurfaceWin(
      HWND parent_window,
      VSyncCallback vsync_callback,
      const DirectCompositionSurfaceWin::Settings& settings);

  // Returns true if direct composition is supported.  We prefer to use direct
  // composition even without hardware overlays, because it allows us to bypass
  // blitting by DWM to the window redirection surface by using a flip mode swap
  // chain.  Overridden with --disable-direct-composition.
  static bool IsDirectCompositionSupported();

  // Returns true if video overlays are supported and should be used. Overridden
  // with --enable-direct-composition-video-overlays and
  // --disable-direct-composition-video-overlays. This function is thread safe.
  static bool AreOverlaysSupported();

  // Returns true if zero copy decode swap chain is supported.
  static bool IsDecodeSwapChainSupported();
  static void DisableDecodeSwapChain();

  // After this is called, overlay support is disabled during the
  // current GPU process' lifetime.
  static void DisableOverlays();

  // Indicate the overlay caps are invalid.
  static void InvalidateOverlayCaps();

  // Returns true if scaled hardware overlays are supported.
  static bool AreScaledOverlaysSupported();

  // Returns preferred overlay format set when detecting overlay support.
  static DXGI_FORMAT GetOverlayFormatUsedForSDR();

  // Returns monitor size.
  static gfx::Size GetOverlayMonitorSize();

  // Returns overlay support flags for the given format.
  // Caller should check for DXGI_OVERLAY_SUPPORT_FLAG_DIRECT and
  // DXGI_OVERLAY_SUPPORT_FLAG_SCALING bits.
  // This function is thread safe.
  static UINT GetOverlaySupportFlags(DXGI_FORMAT format);

  // Returns true if there is an HDR capable display connected.
  static bool IsHDRSupported();

  // Returns true if swap chain tearing is supported.
  static bool IsSwapChainTearingSupported();

  static bool AllowTearing();

  static void SetScaledOverlaysSupportedForTesting(bool value);

  static void SetOverlayFormatUsedForTesting(DXGI_FORMAT format);

  static void SetOverlayHDRGpuInfoUpdateCallback(
      OverlayHDRInfoUpdateCallback callback);

  // On Intel GPUs where YUV overlays are supported, BGRA8 overlays are
  // supported as well but IDXGIOutput3::CheckOverlaySupport() returns
  // unsupported. So allow manually enabling BGRA8 overlay support.
  static void EnableBGRA8OverlaysWithYUVOverlaySupport();

  // Forces to enable NV12 overlay support regardless of the query results from
  // IDXGIOutput3::CheckOverlaySupport().
  static void ForceNV12OverlaySupport();

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
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback callback) override;
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
  bool ScheduleDCLayer(const ui::DCRendererLayerParams& params) override;
  void SetFrameRate(float frame_rate) override;

  // Implements GpuSwitchingObserver.
  void OnGpuSwitched(gl::GpuPreference active_gpu_heuristic) override;
  void OnDisplayAdded() override;
  void OnDisplayRemoved() override;

  HWND window() const { return window_; }

  scoped_refptr<base::TaskRunner> GetWindowTaskRunnerForTesting();

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetLayerSwapChainForTesting(
      size_t index) const;

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetBackbufferSwapChainForTesting()
      const;

  scoped_refptr<DirectCompositionChildSurfaceWin> GetRootSurfaceForTesting()
      const;

 protected:
  ~DirectCompositionSurfaceWin() override;

 private:
  HWND window_ = nullptr;
  ChildWindowWin child_window_;

  scoped_refptr<DirectCompositionChildSurfaceWin> root_surface_;
  std::unique_ptr<DCLayerTree> layer_tree_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;

  DISALLOW_COPY_AND_ASSIGN(DirectCompositionSurfaceWin);
};

}  // namespace gl

#endif  // UI_GL_DIRECT_COMPOSITION_SURFACE_WIN_H_
