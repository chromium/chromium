// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_
#define UI_GL_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_

#include <windows.h>
#include <d3d11.h>
#include <dcomp.h>
#include <wrl/client.h>

#include "ui/gl/gl_surface_egl.h"

namespace gl {

class DirectCompositionChildSurfaceWin : public GLSurfaceEGL {
 public:
  DirectCompositionChildSurfaceWin();

  // GLSurfaceEGL implementation.
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  gfx::Size GetSize() override;
  bool IsOffscreen() override;
  void* GetHandle() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  bool FlipsVertically() const override;
  bool SupportsPostSubBuffer() override;
  bool OnMakeCurrent(GLContext* context) override;
  bool SupportsDCLayers() const override;
  bool SetDrawRectangle(const gfx::Rect& rect) override;
  gfx::Vector2d GetDrawOffset() const override;
  void SetVSyncEnabled(bool enabled) override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  bool SetEnableDCLayers(bool enable) override;

  const Microsoft::WRL::ComPtr<IDCompositionSurface>& dcomp_surface() const {
    return dcomp_surface_;
  }

  const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }

  uint64_t dcomp_surface_serial() const { return dcomp_surface_serial_; }

 protected:
  ~DirectCompositionChildSurfaceWin() override;

 private:
  // Release the texture that's currently being drawn to. If will_discard is
  // true then the surface should be discarded without swapping any contents
  // to it. Returns false if this fails.
  bool ReleaseDrawTexture(bool will_discard);

  gfx::Size size_ = gfx::Size(1, 1);
  bool enable_dc_layers_ = false;
  bool has_alpha_ = true;
  bool vsync_enabled_ = true;
  ColorSpace color_space_ = ColorSpace::UNSPECIFIED;

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

  DISALLOW_COPY_AND_ASSIGN(DirectCompositionChildSurfaceWin);
};

}  // namespace gl

#endif  // UI_GL_DIRECT_COMPOSITION_CHILD_SURFACE_WIN_H_
