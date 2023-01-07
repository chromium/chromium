// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_NATIVE_PIXMAP_GL_BINDING_H_
#define UI_OZONE_PUBLIC_NATIVE_PIXMAP_GL_BINDING_H_

#include "base/component_export.h"
#include "ui/gl/gl_image.h"

typedef unsigned int GLuint;
typedef unsigned int GLenum;

namespace ui {

// A binding maintained between NativePixmap and GL Texture in Ozone.
class COMPONENT_EXPORT(OZONE_BASE) NativePixmapGLBinding {
 public:
  NativePixmapGLBinding();
  virtual ~NativePixmapGLBinding();

  GLuint GetInternalFormat();
  GLenum GetDataFormat();
  GLenum GetDataType();

 protected:
  bool BindTexture(scoped_refptr<gl::GLImage>,
                   GLenum target,
                   GLuint texture_id);

 private:
  // TODO(hitawala): Merge BindTexImage, Initialize from GLImage and its
  // subclasses {NativePixmap, GLXNativePixmap} to NativePixmapGLBinding and its
  // subclasses once we stop using them elsewhere eg. VDA decoders in media.
  scoped_refptr<gl::GLImage> gl_image_;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_NATIVE_PIXMAP_GL_BINDING_H_
