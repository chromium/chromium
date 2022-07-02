// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/gl_ozone_egl_x11.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/ozone/platform/x11/native_pixmap_egl_x11_binding.h"

namespace ui {

GLOzoneEGLX11::GLOzoneEGLX11() = default;
GLOzoneEGLX11::~GLOzoneEGLX11() = default;

bool GLOzoneEGLX11::CanImportNativePixmap() {
  return gl::GLSurfaceEGL::GetGLDisplayEGL()
      ->ext->b_EGL_NOK_texture_from_pixmap;
}

std::unique_ptr<NativePixmapGLBinding> GLOzoneEGLX11::ImportNativePixmap(
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat plane_format,
    gfx::BufferPlane plane,
    gfx::Size plane_size,
    const gfx::ColorSpace& color_space,
    GLenum target,
    GLuint texture_id) {
  return NativePixmapEGLX11Binding::Create(pixmap, plane_format, plane_size,
                                           target, texture_id);
}

}  // namespace ui
