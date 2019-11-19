// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_X11_H_
#define UI_GL_GL_SURFACE_EGL_X11_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

// Encapsulates an EGL surface bound to a view using the X Window System.
class GL_EXPORT NativeViewGLSurfaceEGLX11 : public NativeViewGLSurfaceEGL {
 public:
  explicit NativeViewGLSurfaceEGLX11(EGLNativeWindowType window);

  // NativeViewGLSurfaceEGL overrides.
  EGLConfig GetConfig() override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  bool InitializeNativeWindow() override;

 private:
  ~NativeViewGLSurfaceEGLX11() override;

  EGLNativeWindowType parent_window_;

  // PlatformEventDispatcher implementation.
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  DISALLOW_COPY_AND_ASSIGN(NativeViewGLSurfaceEGLX11);
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_X11_H_
