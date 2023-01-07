// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_
#define UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_

#include "ui/ozone/public/native_pixmap_gl_binding.h"

namespace ui {

// A binding maintained between GLImageEGLPixmap and GL Textures in Ozone. This
// is used on X11.
class NativePixmapEGLX11Binding : public NativePixmapGLBinding {
 public:
  NativePixmapEGLX11Binding();
  ~NativePixmapEGLX11Binding() override;

  static std::unique_ptr<NativePixmapGLBinding> Create(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::Size plane_size,
      GLenum target,
      GLuint texture_id);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_
