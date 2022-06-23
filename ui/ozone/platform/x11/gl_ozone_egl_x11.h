// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_GL_OZONE_EGL_X11_H_
#define UI_OZONE_PLATFORM_X11_GL_OZONE_EGL_X11_H_

#include "ui/ozone/common/gl_ozone_egl.h"

namespace ui {

// A partial implementation of GLOzone for EGL on X11. This implementation is
// used when ANGLE supports pixmaps through eglCreatePixmapSurface and exposes
// it through EGL extension EGL_NOK_texture_from_pixmap to chrome. This can then
// be used to create native bindings using GLImageEGLPixmap.
class GLOzoneEGLX11 : public GLOzoneEGL {
 public:
  GLOzoneEGLX11();

  GLOzoneEGLX11(const GLOzoneEGLX11&) = delete;
  GLOzoneEGLX11& operator=(const GLOzoneEGLX11&) = delete;

  ~GLOzoneEGLX11() override;

  bool CanImportNativePixmap() override;
  std::unique_ptr<NativePixmapGLBinding> ImportNativePixmap(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::BufferPlane plane,
      gfx::Size plane_size,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id) override;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_GL_OZONE_EGL_X11_H_
