// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_GL_SURFACE_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_GL_SURFACE_WAYLAND_H_

#include <memory>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_surface_egl.h"

struct wl_egl_window;

namespace ui {

class WaylandWindow;

struct EGLWindowDeleter {
  void operator()(wl_egl_window* egl_window);
};

std::unique_ptr<wl_egl_window, EGLWindowDeleter> CreateWaylandEglWindow(
    WaylandWindow* window);

// GLSurface class implementation for wayland.
class GLSurfaceWayland : public gl::NativeViewGLSurfaceEGL {
 public:
  using WaylandEglWindowPtr = std::unique_ptr<wl_egl_window, EGLWindowDeleter>;

  explicit GLSurfaceWayland(WaylandEglWindowPtr egl_window);

  // gl::GLSurface:
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  EGLConfig GetConfig() override;

 private:
  ~GLSurfaceWayland() override;

  WaylandEglWindowPtr egl_window_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceWayland);
};

}  // namespace ui
#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_GL_SURFACE_WAYLAND_H_
