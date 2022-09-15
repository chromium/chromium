// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_GL_SURFACE_EGL_READBACK_X11_H_
#define UI_OZONE_PLATFORM_X11_GL_SURFACE_EGL_READBACK_X11_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/x/xproto.h"
#include "ui/ozone/common/gl_surface_egl_readback.h"

namespace ui {

// GLSurface implementation that copies pixels from readback to an XWindow.
class GLSurfaceEglReadbackX11 : public GLSurfaceEglReadback {
 public:
  GLSurfaceEglReadbackX11(gl::GLDisplayEGL* display,
                          gfx::AcceleratedWidget window);

  GLSurfaceEglReadbackX11(const GLSurfaceEglReadbackX11&) = delete;
  GLSurfaceEglReadbackX11& operator=(const GLSurfaceEglReadbackX11&) = delete;

  // gl::GLSurface:
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;

 private:
  ~GLSurfaceEglReadbackX11() override;

  // gl::GLSurfaceEglReadback:
  bool HandlePixels(uint8_t* pixels) override;

  const x11::Window window_;
  const raw_ptr<x11::Connection> connection_;
  x11::GraphicsContext window_graphics_context_{};
  x11::VisualId visual_{};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_GL_SURFACE_EGL_READBACK_X11_H_
