// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_EGL_PIXMAP_H_
#define UI_GL_GL_IMAGE_EGL_PIXMAP_H_

#include <stdint.h>

#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/glx.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

typedef void* EGLSurface;
typedef void* EGLDisplay;

namespace media {
class VaapiPictureNativePixmapAngle;
}

namespace ui {
class NativePixmapEGLX11Binding;
}

namespace gl {

class GL_EXPORT GLImageEGLPixmap : public GLImage {
  // NOTE: We are in the process of eliminating this class, so no new usages
  // of it should be introduced.
 private:
  friend class media::VaapiPictureNativePixmapAngle;
  friend class ui::NativePixmapEGLX11Binding;

  GLImageEGLPixmap(const gfx::Size& size, gfx::BufferFormat format);

  GLImageEGLPixmap(const GLImageEGLPixmap&) = delete;
  GLImageEGLPixmap& operator=(const GLImageEGLPixmap&) = delete;

  bool Initialize(x11::Pixmap pixmap);

  // Overridden from GLImage:
  gfx::Size GetSize() override;

  // Binds image to texture currently bound to |target|. Returns true on
  // success.
  bool BindTexImage(unsigned target);

  // Releases the image that was bound via BindTexImage().
  void ReleaseEGLImage();

  ~GLImageEGLPixmap() override;

  gfx::BufferFormat format() const { return format_; }

  EGLSurface surface_;
  const gfx::Size size_;
  gfx::BufferFormat format_;
  EGLDisplay display_;
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_EGL_PIXMAP_H_
