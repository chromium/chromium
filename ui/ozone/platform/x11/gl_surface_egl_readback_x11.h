// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_GL_SURFACE_EGL_READBACK_X11_H_
#define UI_OZONE_PLATFORM_X11_GL_SURFACE_EGL_READBACK_X11_H_

#include "ui/gfx/x/xproto.h"
#include "ui/ozone/common/gl_surface_egl_readback.h"

namespace ui {

// GLSurface implementation that copies pixels from readback to an XWindow.
class GLSurfaceEglReadbackX11 : public GLSurfaceEglReadback {
 public:
  explicit GLSurfaceEglReadbackX11(gfx::AcceleratedWidget window);

  // gl::GLSurface:
  bool Initialize(gl::GLSurfaceFormat format) override;
  void Destroy() override;

 private:
  ~GLSurfaceEglReadbackX11() override;

  // gl::GLSurfaceEglReadback:
  bool HandlePixels(uint8_t* pixels) override;

  const x11::Window window_;
  x11::Connection* const connection_;
  x11::GraphicsContext window_graphics_context_{};
  x11::VisualId visual_{};

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceEglReadbackX11);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_GL_SURFACE_EGL_READBACK_X11_H_
