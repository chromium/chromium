// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_GL_RENDERER_H_
#define UI_OZONE_DEMO_GL_RENDERER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/demo/renderer_base.h"

namespace gfx {
struct PresentationFeedback;
}  // namespace gfx

namespace gl {
class GLContext;
class GLSurface;
}  // namespace gl

namespace ui {
class PlatformWindowSurface;

class GlRenderer : public RendererBase {
 public:
  GlRenderer(gfx::AcceleratedWidget widget,
             std::unique_ptr<PlatformWindowSurface> platform_window_surface,
             const scoped_refptr<gl::GLSurface>& surface,
             const gfx::Size& size);

  GlRenderer(const GlRenderer&) = delete;
  GlRenderer& operator=(const GlRenderer&) = delete;

  ~GlRenderer() override;

  // Renderer:
  bool Initialize() override;

 private:
  void RenderFrame();
  void PostRenderFrameTask(gfx::SwapCompletionResult result);
  void OnPresentation(const gfx::PresentationFeedback& feedback);

  std::unique_ptr<PlatformWindowSurface> window_surface_;

  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> context_;

  base::WeakPtrFactory<GlRenderer> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_GL_RENDERER_H_
