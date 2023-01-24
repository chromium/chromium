// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_EGL_PIXMAP_H_
#define UI_GL_GL_IMAGE_EGL_PIXMAP_H_

#include <stdint.h>

#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/glx.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

typedef void* EGLSurface;
typedef void* EGLDisplay;

namespace gl {

class GL_EXPORT GLImageEGLPixmap : public GLImage {
 public:
  GLImageEGLPixmap(const gfx::Size& size, gfx::BufferFormat format);

  GLImageEGLPixmap(const GLImageEGLPixmap&) = delete;
  GLImageEGLPixmap& operator=(const GLImageEGLPixmap&) = delete;

  bool Initialize(x11::Pixmap pixmap);

  // Overridden from GLImage:
  gfx::Size GetSize() override;
  bool BindTexImage(unsigned target) override;

  // Releases the image that was bound via BindTexImage().
  void ReleaseEGLImage();

 protected:
  ~GLImageEGLPixmap() override;

  gfx::BufferFormat format() const { return format_; }

 private:
  EGLSurface surface_;
  const gfx::Size size_;
  gfx::BufferFormat format_;
  EGLDisplay display_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_EGL_PIXMAP_H_
