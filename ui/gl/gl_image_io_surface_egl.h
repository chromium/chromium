// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_IO_SURFACE_EGL_H_
#define UI_GL_GL_IMAGE_IO_SURFACE_EGL_H_

#include "ui/gl/gl_image_io_surface.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <map>

namespace gl {

class GLDisplayEGL;
class EGLAccess;

class EGLAccess {
 public:
  explicit EGLAccess(GLDisplayEGL* display);
  ~EGLAccess();

  const GLDisplayEGL* display() { return display_; }
  EGLConfig dummy_config() { return dummy_config_; }
  EGLint texture_target() { return texture_target_; }
  EGLSurface pbuffer() { return pbuffer_; }

  void set_pbuffer(EGLSurface pbuffer) { pbuffer_ = pbuffer; }

 private:
  raw_ptr<GLDisplayEGL> display_ = nullptr;
  EGLConfig dummy_config_ = EGL_NO_CONFIG_KHR;
  EGLint texture_target_ = EGL_NO_TEXTURE;
  EGLSurface pbuffer_ = EGL_NO_SURFACE;
};

// Implements a IOSurface-backed GLImage that uses the
// EGL_ANGLE_iosurface_client_buffer extension to bind the IOSurface to textures
class GL_EXPORT GLImageIOSurfaceEGL : public GLImageIOSurface {
 public:
  GLImageIOSurfaceEGL(const gfx::Size& size, unsigned internalformat);

  void ReleaseTexImage(unsigned target) override;

 protected:
  ~GLImageIOSurfaceEGL() override;
  bool BindTexImageImpl(unsigned target, unsigned internalformat) override;

 private:
  EGLAccess& GetEGLAccessForCurrentContext();

  std::map<const GLDisplayEGL*, EGLAccess> egl_access_map_;
  bool texture_bound_ = false;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_IO_SURFACE_EGL_H_
