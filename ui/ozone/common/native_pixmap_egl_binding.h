// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_NATIVE_PIXMAP_EGL_BINDING_H_
#define UI_OZONE_COMMON_NATIVE_PIXMAP_EGL_BINDING_H_

#include "ui/ozone/public/native_pixmap_gl_binding.h"

namespace ui {

// A binding maintained between GLImageNativePixmap and GL Textures in Ozone.
class NativePixmapEGLBinding : public NativePixmapGLBinding {
 public:
  NativePixmapEGLBinding();
  ~NativePixmapEGLBinding() override;

  static std::unique_ptr<NativePixmapGLBinding> Create(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::BufferPlane plane,
      gfx::Size plane_size,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_NATIVE_PIXMAP_EGL_BINDING_H_
