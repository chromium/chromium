// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_
#define UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_

#include <memory>

#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/demo/gl_renderer.h"

namespace gl {
class Presenter;
}

namespace ui {
class OverlayCandidatesOzone;
class NativePixmapGLBinding;
class PlatformWindowSurface;

static const int kMaxLayers = 8;

class SurfacelessGlRenderer : public RendererBase {
 public:
  SurfacelessGlRenderer(gfx::AcceleratedWidget widget,
                        std::unique_ptr<PlatformWindowSurface> window_surface,
                        const scoped_refptr<gl::GLSurface>& offscreen_surface,
                        const scoped_refptr<gl::Presenter>& presenter,
                        const gfx::Size& size);

  SurfacelessGlRenderer(const SurfacelessGlRenderer&) = delete;
  SurfacelessGlRenderer& operator=(const SurfacelessGlRenderer&) = delete;

  ~SurfacelessGlRenderer() override;

  // Renderer:
  bool Initialize() override;

 private:
  void RenderFrame();
  void PostRenderFrameTask(gfx::SwapCompletionResult result);
  void OnPresentation(const gfx::PresentationFeedback& feedback);

  class BufferWrapper {
   public:
    BufferWrapper();
    ~BufferWrapper();

    scoped_refptr<gfx::NativePixmap> image() const;

    bool Initialize(gfx::AcceleratedWidget widget, const gfx::Size& size);
    void BindFramebuffer();

    const gfx::Size size() const { return size_; }

   private:
    gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
    gfx::Size size_;

    scoped_refptr<gfx::NativePixmap> pixmap_;
    std::unique_ptr<NativePixmapGLBinding> pixmap_gl_binding_;
    unsigned int gl_fb_ = 0;
    unsigned int gl_tex_ = 0;
  };

  std::unique_ptr<BufferWrapper> buffers_[2];

  std::unique_ptr<BufferWrapper> overlay_buffers_[kMaxLayers][2];
  size_t overlay_cnt_ = 0;
  bool disable_primary_plane_ = false;
  gfx::Rect primary_plane_rect_;
  bool use_gpu_fences_ = false;

  std::unique_ptr<OverlayCandidatesOzone> overlay_checker_;

  int back_buffer_ = 0;

  std::unique_ptr<PlatformWindowSurface> window_surface_;

  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> context_;

  scoped_refptr<gl::Presenter> presenter_;

  base::WeakPtrFactory<SurfacelessGlRenderer> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_SURFACELESS_GL_RENDERER_H_
