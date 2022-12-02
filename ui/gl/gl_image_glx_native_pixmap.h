// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_GLX_NATIVE_PIXMAP_H_
#define UI_GL_GL_IMAGE_GLX_NATIVE_PIXMAP_H_

#include "base/memory/scoped_refptr.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image_glx.h"

namespace gl {

class GL_EXPORT GLImageGLXNativePixmap : public GLImageGLX {
 public:
  GLImageGLXNativePixmap(const gfx::Size& size,
                         gfx::BufferFormat format,
                         gfx::BufferPlane plane = gfx::BufferPlane::DEFAULT);

  GLImageGLXNativePixmap(const GLImageGLXNativePixmap&) = delete;
  GLImageGLXNativePixmap& operator=(const GLImageGLXNativePixmap&) = delete;

  bool Initialize(scoped_refptr<gfx::NativePixmap> pixmap);

  static bool CanImportNativePixmap();

 protected:
  ~GLImageGLXNativePixmap() override;

 private:
  scoped_refptr<gfx::NativePixmap> native_pixmap_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_GLX_NATIVE_PIXMAP_H_
