// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_SKIA_SKIA_GL_RENDERER_H_
#define UI_OZONE_DEMO_SKIA_SKIA_GL_RENDERER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/simple_thread.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/private/chromium/GrDeferredDisplayListRecorder.h"
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

class SkiaGlRenderer : public RendererBase,
                       public base::DelegateSimpleThread::Delegate {
 public:
  SkiaGlRenderer(gfx::AcceleratedWidget widget,
                 std::unique_ptr<PlatformWindowSurface> window_surface,
                 const scoped_refptr<gl::GLSurface>& gl_surface,
                 const gfx::Size& size);

  SkiaGlRenderer(const SkiaGlRenderer&) = delete;
  SkiaGlRenderer& operator=(const SkiaGlRenderer&) = delete;

  ~SkiaGlRenderer() override;

  // Renderer:
  bool Initialize() override;

 protected:
  virtual void RenderFrame();
  virtual void PostRenderFrameTask(gfx::SwapCompletionResult result);

  void Draw(SkCanvas* canvas, float fraction);
  void StartDDLRenderThreadIfNecessary(SkSurface* sk_surface);
  void StopDDLRenderThread();
  sk_sp<GrDeferredDisplayList> GetDDL();

  std::unique_ptr<PlatformWindowSurface> window_surface_;

  scoped_refptr<gl::GLSurface> gl_surface_;
  scoped_refptr<gl::GLContext> gl_context_;

  sk_sp<GrDirectContext> gr_context_;
  const bool use_ddl_;

 private:
  // base::DelegateSimpleThread::Delegate:
  void Run() override;

  void OnPresentation(const gfx::PresentationFeedback& feedback);

  sk_sp<SkSurface> sk_surface_;

  float rotation_angle_ = 0.f;

  std::unique_ptr<base::SimpleThread> ddl_render_thread_;

  // The lock to protect |surface_charaterization_| and |ddls_|.
  base::Lock lock_;

  // The condition variable for signalling change of |ddls_|.
  base::ConditionVariable condition_variable_;

  GrSurfaceCharacterization surface_charaterization_;
  base::queue<sk_sp<GrDeferredDisplayList>> ddls_;

  base::WeakPtrFactory<SkiaGlRenderer> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_SKIA_SKIA_GL_RENDERER_H_
