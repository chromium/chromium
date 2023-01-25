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
class GLImageNativePixmap;
}

namespace ui {

// A binding maintained between GLImageNativePixmap and GL Textures in Ozone.
class NativePixmapEGLBinding : public NativePixmapGLBinding {
 public:
  NativePixmapEGLBinding(scoped_refptr<gl::GLImageNativePixmap> gl_image,
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
  // Invokes NativePixmapGLBinding::BindTexture, passing |gl_image_|.
  bool BindTexture(GLenum target, GLuint texture_id);

  // TODO(hitawala): Merge BindTexImage, Initialize from GLImage and its
  // subclass NativePixmap to NativePixmapEGLBinding once we stop using them
  // elsewhere eg. VDA decoders in media.
  scoped_refptr<gl::GLImageNativePixmap> gl_image_;

  gfx::BufferFormat format_;
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_NATIVE_PIXMAP_EGL_BINDING_H_
