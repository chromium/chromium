// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/gl_surface_wayland.h"

#include <wayland-egl.h>
#include <memory>
#include <utility>

#include "third_party/khronos/EGL/egl.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

void EGLWindowDeleter::operator()(wl_egl_window* egl_window) {
  wl_egl_window_destroy(egl_window);
}

std::unique_ptr<wl_egl_window, EGLWindowDeleter> CreateWaylandEglWindow(
    WaylandWindow* window) {
  gfx::Size size = window->GetBounds().size();
  return std::unique_ptr<wl_egl_window, EGLWindowDeleter>(
      wl_egl_window_create(window->surface(), size.width(), size.height()));
}

GLSurfaceWayland::GLSurfaceWayland(WaylandEglWindowPtr egl_window)
    : NativeViewGLSurfaceEGL(
          reinterpret_cast<EGLNativeWindowType>(egl_window.get()),
          nullptr),
      egl_window_(std::move(egl_window)) {
  DCHECK(egl_window_);
}

bool GLSurfaceWayland::Resize(const gfx::Size& size,
                              float scale_factor,
                              ColorSpace color_space,
                              bool has_alpha) {
  if (size_ == size)
    return true;
  wl_egl_window_resize(egl_window_.get(), size.width(), size.height(), 0, 0);
  size_ = size;
  return true;
}

EGLConfig GLSurfaceWayland::GetConfig() {
  if (!config_) {
    GLint config_attribs[] = {EGL_BUFFER_SIZE,
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
    config_ = ChooseEGLConfig(GetDisplay(), config_attribs);
  }
  return config_;
}

GLSurfaceWayland::~GLSurfaceWayland() {
  Destroy();
}

}  // namespace ui
