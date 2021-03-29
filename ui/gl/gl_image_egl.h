// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_IMAGE_EGL_H_
#define UI_GL_GL_IMAGE_EGL_H_

#include <EGL/eglplatform.h>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"

namespace gl {

// Abstract base class for EGL-based images.
class GL_EXPORT GLImageEGL : public GLImage {
 public:
  explicit GLImageEGL(const gfx::Size& size);

  // Overridden from GLImage:
  gfx::Size GetSize() override;
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override {}

 protected:
  ~GLImageEGL() override;

  // Same semantic as specified for eglCreateImageKHR. There two main usages:
  // 1- When using the |target| EGL_GL_TEXTURE_2D_KHR it is required to pass
  // a valid |context|. This allows to create an EGLImage from a GL texture.
  // Then this EGLImage can be converted to an external resource to be shared
  // with other client APIs.
  // 2- When using the |target| EGL_NATIVE_PIXMAP_KHR or EGL_LINUX_DMA_BUF_EXT
  // it is required to pass EGL_NO_CONTEXT. This allows to create an EGLImage
  // from an external resource. Then this EGLImage can be converted to a GL
  // texture.
  bool Initialize(void* context /* EGLContext */,
                  unsigned target /* EGLenum */,
                  void* buffer /* EGLClientBuffer */,
                  const EGLint* attrs);

  void* egl_image_ /* EGLImageKHR */;
  const gfx::Size size_;
  base::ThreadChecker thread_checker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GLImageEGL);
};

}  // namespace gl

#endif  // UI_GL_GL_IMAGE_EGL_H_
