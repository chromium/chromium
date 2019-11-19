// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/demo/gl_renderer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace ui {

GlRenderer::GlRenderer(gfx::AcceleratedWidget widget,
                       std::unique_ptr<PlatformWindowSurface> window_surface,
                       const scoped_refptr<gl::GLSurface>& gl_surface,
                       const gfx::Size& size)
    : RendererBase(widget, size),
      window_surface_(std::move(window_surface)),
      gl_surface_(gl_surface) {}

GlRenderer::~GlRenderer() {}

bool GlRenderer::Initialize() {
  context_ = gl::init::CreateGLContext(nullptr, gl_surface_.get(),
                                       gl::GLContextAttribs());
  if (!context_.get()) {
    LOG(ERROR) << "Failed to create GL context";
    return false;
  }

  gl_surface_->Resize(size_, 1.f, gl::GLSurface::ColorSpace::UNSPECIFIED, true);

  if (!context_->MakeCurrent(gl_surface_.get())) {
    LOG(ERROR) << "Failed to make GL context current";
    return false;
  }

  // Schedule the initial render.
  PostRenderFrameTask(gfx::SwapResult::SWAP_ACK, nullptr);
  return true;
}

void GlRenderer::RenderFrame() {
  TRACE_EVENT0("ozone", "GlRenderer::RenderFrame");

  float fraction = NextFraction();

  context_->MakeCurrent(gl_surface_.get());

  glViewport(0, 0, size_.width(), size_.height());
  glClearColor(1.f - fraction, fraction, 0.f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  if (gl_surface_->SupportsAsyncSwap()) {
    gl_surface_->SwapBuffersAsync(
        base::BindOnce(&GlRenderer::PostRenderFrameTask,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&GlRenderer::OnPresentation,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    PostRenderFrameTask(
        gl_surface_->SwapBuffers(base::BindOnce(
            &GlRenderer::OnPresentation, weak_ptr_factory_.GetWeakPtr())),
        nullptr);
  }
}

void GlRenderer::PostRenderFrameTask(gfx::SwapResult result,
                                     std::unique_ptr<gfx::GpuFence> gpu_fence) {
  if (gpu_fence)
    gpu_fence->Wait();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&GlRenderer::RenderFrame, weak_ptr_factory_.GetWeakPtr()));
}

void GlRenderer::OnPresentation(const gfx::PresentationFeedback& feedback) {
  LOG_IF(ERROR, feedback.timestamp.is_null()) << "Last frame is discarded!";
}

}  // namespace ui
