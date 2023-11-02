// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_egl.h"

#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

GLImageEGL::GLImageEGL(const gfx::Size& size)
    : egl_image_(EGL_NO_IMAGE_KHR), size_(size) {}

GLImageEGL::~GLImageEGL() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (egl_image_ == EGL_NO_IMAGE_KHR)
    return;

  const EGLBoolean result = eglDestroyImageKHR(
      GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(), egl_image_);
  if (result == EGL_FALSE)
    DLOG(ERROR) << "Error destroying EGLImage: " << ui::GetLastEGLErrorString();
}

bool GLImageEGL::Initialize(EGLContext context,
                            EGLenum target,
                            EGLClientBuffer buffer,
                            const EGLint* attrs) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(EGL_NO_IMAGE_KHR, egl_image_);
  egl_image_ = eglCreateImageKHR(GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(),
                                 context, target, buffer, attrs);
  const bool success = egl_image_ != EGL_NO_IMAGE_KHR;
  if (!success)
    LOG(ERROR) << "Error creating EGLImage: " << ui::GetLastEGLErrorString();
  return success;
}

gfx::Size GLImageEGL::GetSize() {
  return size_;
}

void* GLImageEGL::GetEGLImage() const {
  return egl_image_;
}

GLImageEGL::BindOrCopy GLImageEGL::ShouldBindOrCopy() {
  return egl_image_ == EGL_NO_IMAGE_KHR ? COPY : BIND;
}

bool GLImageEGL::BindTexImage(unsigned target) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(BIND, ShouldBindOrCopy());

  glEGLImageTargetTexture2DOES(target, egl_image_);
  return true;
}

}  // namespace gl
