// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_NATIVE_PIXMAP_EGL_BINDING_HELPER_H_
#define UI_GL_NATIVE_PIXMAP_EGL_BINDING_HELPER_H_

#include <stdint.h>

#include "base/threading/thread_checker.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/scoped_egl_image.h"

typedef unsigned int GLuint;
typedef unsigned int GLenum;

namespace gl {

class GL_EXPORT NativePixmapEGLBindingHelper {
 public:
  // Create an EGLImage from a given NativePixmap and plane and bind
  // |texture_id| to |target| followed by binding the image to |target|. The
  // color space is for the external sampler: When we sample the YUV buffer as
  // RGB, we need to tell it the encoding (BT.601, BT.709, or BT.2020) and range
  // (limited or null), and |color_space| conveys this.
  static std::unique_ptr<NativePixmapEGLBindingHelper> CreateForPlane(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferPlane plane,
      scoped_refptr<gfx::NativePixmap> pixmap,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id);

  NativePixmapEGLBindingHelper(const gfx::Size& size,
                               gfx::BufferFormat format,
                               gfx::BufferPlane plane);
  ~NativePixmapEGLBindingHelper();

  // Get the GL internal format of the image.
  // It is aligned with glTexImage{2|3}D's parameter |internalformat|.
  unsigned GetInternalFormat();

 private:
  // Create an EGLImage from a given NativePixmap and bind |texture_id| to
  // |target| followed by binding the image to |target|. This EGLImage can be
  // converted to a GL texture.
  bool InitializeFromNativePixmap(scoped_refptr<gfx::NativePixmap> pixmap,
                                  const gfx::ColorSpace& color_space,
                                  GLenum target,
                                  GLuint texture_id);

  ScopedEGLImage egl_image_;
  const gfx::Size size_;
  THREAD_CHECKER(thread_checker_);
  gfx::BufferFormat format_;
  scoped_refptr<gfx::NativePixmap> pixmap_;
  gfx::BufferPlane plane_;
};

}  // namespace gl

#endif  // UI_GL_NATIVE_PIXMAP_EGL_BINDING_HELPER_H_
