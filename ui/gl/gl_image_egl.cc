// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_egl.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
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

  const EGLBoolean result =
      eglDestroyImageKHR(GLSurfaceEGL::GetHardwareDisplay(), egl_image_);
  if (result == EGL_FALSE)
    DLOG(ERROR) << "Error destroying EGLImage: " << ui::GetLastEGLErrorString();
}

bool GLImageEGL::Initialize(EGLContext context,
                            EGLenum target,
                            EGLClientBuffer buffer,
                            const EGLint* attrs) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(EGL_NO_IMAGE_KHR, egl_image_);
  egl_image_ = eglCreateImageKHR(GLSurfaceEGL::GetHardwareDisplay(), context,
                                 target, buffer, attrs);
  const bool success = egl_image_ != EGL_NO_IMAGE_KHR;
  if (!success)
    LOG(ERROR) << "Error creating EGLImage: " << ui::GetLastEGLErrorString();
  return success;
}

gfx::Size GLImageEGL::GetSize() {
  return size_;
}

GLImageEGL::BindOrCopy GLImageEGL::ShouldBindOrCopy() {
  return egl_image_ == EGL_NO_IMAGE_KHR ? COPY : BIND;
}

bool GLImageEGL::BindTexImage(unsigned target) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(BIND, ShouldBindOrCopy());

  glEGLImageTargetTexture2DOES(target, egl_image_);
  base::ElapsedTimer thread_blocked_timer;
  const GLenum error = glGetError();
  UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
      "Gpu.GL.GetErrorDuration.GLImageEGL.BindTexImage",
      thread_blocked_timer.Elapsed(), base::Microseconds(10),
      base::Milliseconds(16), 100);

  DLOG_IF(ERROR, error != GL_NO_ERROR)
      << "Error binding EGLImage: " << GLEnums::GetStringError(error);

  UMA_HISTOGRAM_BOOLEAN("Gpu.GL.GetErrorResult.GLImageEGL.BindTexImage",
                        error != GL_NO_ERROR);
  return error == GL_NO_ERROR;
}

}  // namespace gl
