// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/gl_surface_wayland.h"

#include <wayland-egl.h>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gl/gl_surface_presentation_helper.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

void EGLWindowDeleter::operator()(wl_egl_window* egl_window) {
  wl_egl_window_destroy(egl_window);
}

std::unique_ptr<wl_egl_window, EGLWindowDeleter> CreateWaylandEglWindow(
    WaylandWindow* window) {
  gfx::Size size = window->applied_state().size_px;
  return std::unique_ptr<wl_egl_window, EGLWindowDeleter>(wl_egl_window_create(
      window->root_surface()->surface(), size.width(), size.height()));
}

GLSurfaceWayland::GLSurfaceWayland(gl::GLDisplayEGL* display,
                                   WaylandEglWindowPtr egl_window,
                                   WaylandWindow* window)
    : NativeViewGLSurfaceEGL(
          display,
          reinterpret_cast<EGLNativeWindowType>(egl_window.get()),
          nullptr),
      egl_window_(std::move(egl_window)),
      window_(window) {
  DCHECK(egl_window_);
  DCHECK(window_);
  window_->root_surface()->ForceImmediateStateApplication();
}

bool GLSurfaceWayland::Resize(const gfx::Size& size,
                              float scale_factor,
                              const gfx::ColorSpace& color_space,
                              bool has_alpha) {
  if (size_ == size) {
    return true;
  }
  wl_egl_window_resize(egl_window_.get(), size.width(), size.height(), 0, 0);
  size_ = size;
  scale_factor_ = ceil(scale_factor);
  return true;
}

EGLConfig GLSurfaceWayland::GetConfig() {
  if (!config_) {
    EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                               32,
                               EGL_ALPHA_SIZE,
                               8,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_RED_SIZE,
                               8,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_SURFACE_TYPE,
                               EGL_WINDOW_BIT,
                               EGL_NONE};
    config_ = ChooseEGLConfig(GetEGLDisplay(), config_attribs);
  }
  return config_;
}

// TODO(crbug.com/368634141): Revisit and double-check the direct root surface
// state setter calls in the functions below, where a cached and ceiled scale
// factor is used. Ideally surface information should be set using the standard
// WaylandFrameManager::ApplySurfaceConfigure flow, i.e: whose entry point is
// WaylandBufferManager::CommitOverlays. Which is done in GbmSurfacelessWayland,
// for example.

gfx::SwapResult GLSurfaceWayland::SwapBuffers(PresentationCallback callback,
                                              gfx::FrameData data) {
  OnSequencePoint(data.seq);
  if (!window_->IsSurfaceConfigured()) {
    // The presentation |callback| must be called after gfx::SwapResult is sent.
    // Thus, use a scoped swap buffers object that will send the feedback later.
    gl::GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
        presentation_helper(), std::move(callback));
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS);
    return scoped_swap_buffers.result();
  }
  window_->root_surface()->set_surface_buffer_scale(scale_factor_);
  return gl::NativeViewGLSurfaceEGL::SwapBuffers(std::move(callback), data);
}

gfx::SwapResult GLSurfaceWayland::PostSubBuffer(int x,
                                                int y,
                                                int width,
                                                int height,
                                                PresentationCallback callback,
                                                gfx::FrameData data) {
  OnSequencePoint(data.seq);
  if (!window_->IsSurfaceConfigured()) {
    // The presentation |callback| must be called after gfx::SwapResult is sent.
    // Thus, use a scoped swap buffers object that will send the feedback later.
    gl::GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
        presentation_helper(), std::move(callback));
    scoped_swap_buffers.set_result(gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS);
    return scoped_swap_buffers.result();
  }
  window_->root_surface()->set_surface_buffer_scale(scale_factor_);
  return gl::NativeViewGLSurfaceEGL::PostSubBuffer(x, y, width, height,
                                                   std::move(callback), data);
}

GLSurfaceWayland::~GLSurfaceWayland() {
  InvalidateWeakPtrs();
  Destroy();
}

void GLSurfaceWayland::OnSequencePoint(int64_t seq) {
  window_->ui_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&WaylandWindow::OnSequencePoint,
                                window_->AsWeakPtr(), seq));
}

}  // namespace ui
