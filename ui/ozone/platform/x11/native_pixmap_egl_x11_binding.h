// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_
#define UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_

#include <memory>

#include "ui/gfx/native_pixmap.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"

namespace gl {
class GLImageEGLPixmap;
}

namespace ui {

// A binding maintained between GLImageEGLPixmap and GL Textures in Ozone. This
// is used on X11.
class NativePixmapEGLX11Binding : public NativePixmapGLBinding {
 public:
  explicit NativePixmapEGLX11Binding(
      scoped_refptr<gl::GLImageEGLPixmap> gl_image,
      gfx::BufferFormat format);
  ~NativePixmapEGLX11Binding() override;

  static std::unique_ptr<NativePixmapGLBinding> Create(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::Size plane_size,
      GLenum target,
      GLuint texture_id);

  // NativePixmapGLBinding:
  GLuint GetInternalFormat() override;
  GLenum GetDataType() override;

 private:
  // Invokes NativePixmapGLBinding::BindTexture, passing |gl_image_|.
  bool BindTexture(GLenum target, GLuint texture_id);

  // TODO(hitawala): Merge BindTexImage, Initialize from GLImage and its
  // subclass EGLPixmap to NativePixmapEGLX11Binding once we stop using them
  // elsewhere eg. VDA decoders in media.
  scoped_refptr<gl::GLImageEGLPixmap> gl_image_;
  gfx::BufferFormat format_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_NATIVE_PIXMAP_EGL_X11_BINDING_H_
