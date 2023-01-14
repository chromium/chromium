// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_GL_SURFACE_WAYLAND_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_GL_SURFACE_WAYLAND_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
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

  GLSurfaceWayland(gl::GLDisplayEGL* display,
                   WaylandEglWindowPtr egl_window,
                   WaylandWindow* window);

  GLSurfaceWayland(const GLSurfaceWayland&) = delete;
  GLSurfaceWayland& operator=(const GLSurfaceWayland&) = delete;

  // gl::GLSurface:
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  EGLConfig GetConfig() override;
  gfx::SwapResult SwapBuffers(PresentationCallback callback,
                              gfx::FrameData data) override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback callback,
                                gfx::FrameData data) override;

 private:
  ~GLSurfaceWayland() override;

  // Delivers sequence number information to WaylandWindow. See the comments
  // on WaylandWindow::applied_state() for more information.
  void OnSequencePoint(int64_t seq);

  WaylandEglWindowPtr egl_window_;
  const raw_ptr<WaylandWindow> window_;

  float scale_factor_ = 1.f;
};

}  // namespace ui
#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_GL_SURFACE_WAYLAND_H_
