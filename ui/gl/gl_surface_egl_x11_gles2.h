// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_X11_GLES2_H_
#define UI_GL_GL_SURFACE_EGL_X11_GLES2_H_

#include <stdint.h>

#include "ui/gfx/x/event.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl_x11.h"

namespace gl {

// Encapsulates an EGL surface bound to a view using the X Window System.
class GL_EXPORT NativeViewGLSurfaceEGLX11GLES2
    : public NativeViewGLSurfaceEGLX11 {
 public:
  explicit NativeViewGLSurfaceEGLX11GLES2(gl::GLDisplayEGL* display,
                                          x11::Window window);

  NativeViewGLSurfaceEGLX11GLES2(const NativeViewGLSurfaceEGLX11GLES2&) =
      delete;
  NativeViewGLSurfaceEGLX11GLES2& operator=(
      const NativeViewGLSurfaceEGLX11GLES2&) = delete;

  // NativeViewGLSurfaceEGL overrides.
  EGLConfig GetConfig() override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              const gfx::ColorSpace& color_space,
              bool has_alpha) override;
  bool InitializeNativeWindow() override;

 protected:
  ~NativeViewGLSurfaceEGLX11GLES2() override;

  x11::Window window() const { return static_cast<x11::Window>(window_); }
  void set_window(x11::Window window) {
    window_ = static_cast<uint32_t>(window);
  }

 private:
  // x11::EventObserver:
  void OnEvent(const x11::Event& xev) override;

  x11::Window parent_window_;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_X11_GLES2_H_
