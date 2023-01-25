// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_NATIVE_PIXMAP_GL_BINDING_H_
#define UI_OZONE_PUBLIC_NATIVE_PIXMAP_GL_BINDING_H_

#include "base/component_export.h"

typedef unsigned int GLuint;
typedef unsigned int GLenum;

namespace gl {
class GLImage;
}

namespace ui {

// A binding maintained between NativePixmap and GL Texture in Ozone.
class COMPONENT_EXPORT(OZONE_BASE) NativePixmapGLBinding {
 public:
  NativePixmapGLBinding();
  virtual ~NativePixmapGLBinding();

  virtual GLuint GetInternalFormat() = 0;
  virtual GLenum GetDataType() = 0;

 protected:
  // Helper method that first binds |texture_id| and subsequently |image| to
  // |target|.
  // NOTE: GLImageNativePixmap::BindTexImage and
  // GLImageNativePixmap::Initialize will be merged to NativePixmapEGLBinding
  // and corresponding code for GLImageEGLPixmap will move to
  // NativePixmapEGLX11Binding leading to the deletion of BindTexture here.
  static bool BindTexture(gl::GLImage* image, GLenum target, GLuint texture_id);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_NATIVE_PIXMAP_GL_BINDING_H_
