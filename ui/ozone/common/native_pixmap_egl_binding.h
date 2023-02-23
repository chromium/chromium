// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_NATIVE_PIXMAP_EGL_BINDING_H_
#define UI_OZONE_COMMON_NATIVE_PIXMAP_EGL_BINDING_H_

#include <memory>

#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"

namespace gfx {
class ColorSpace;
}

namespace gl {
class NativePixmapEGLBindingHelper;
}

namespace ui {

// A binding maintained between NativePixmap and GL Textures in Ozone.
class NativePixmapEGLBinding : public NativePixmapGLBinding {
 public:
  NativePixmapEGLBinding(
      std::unique_ptr<gl::NativePixmapEGLBindingHelper> binding_helper,
      gfx::BufferFormat format);
  ~NativePixmapEGLBinding() override;

  static std::unique_ptr<NativePixmapGLBinding> Create(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::BufferPlane plane,
      gfx::Size plane_size,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id);

  // NativePixmapGLBinding:
  GLuint GetInternalFormat() override;
  GLenum GetDataType() override;

 private:
  std::unique_ptr<gl::NativePixmapEGLBindingHelper> binding_helper_;

  gfx::BufferFormat format_;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_NATIVE_PIXMAP_EGL_BINDING_H_
