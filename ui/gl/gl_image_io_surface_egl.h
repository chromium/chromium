// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_IO_SURFACE_EGL_H_
#define UI_GL_GL_IMAGE_IO_SURFACE_EGL_H_

#include "ui/gl/gl_image_io_surface.h"

#include <EGL/egl.h>

namespace gl {

// Implements a IOSurface-backed GLImage that uses the
// EGL_ANGLE_iosurface_client_buffer extension to bind the IOSurface to textures
class GL_EXPORT GLImageIOSurfaceEGL : public GLImageIOSurface {
 public:
  GLImageIOSurfaceEGL(const gfx::Size& size,
                      unsigned internalformat,
                      bool emulate_rgb);

  void ReleaseTexImage(unsigned target) override;

 protected:
  ~GLImageIOSurfaceEGL() override;
  bool BindTexImageImpl(unsigned target, unsigned internalformat) override;
  bool CopyTexImage(unsigned target) override;

 private:
  // If Swiftshader is being used, use the RGB emulation paths in the validating
  // command decoder instead of creating the IOSurface with an RGB format. ANGLE
  // handles the emulation internally.  This can be removed once Swiftshader is
  // used as an ANGLE backend.
  bool emulate_rgb_;

  EGLDisplay display_;
  EGLSurface pbuffer_;
  EGLConfig dummy_config_;
  EGLint texture_target_;
  bool texture_bound_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_IO_SURFACE_EGL_H_
