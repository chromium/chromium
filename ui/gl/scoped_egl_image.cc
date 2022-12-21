// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_egl_image.h"

#include "base/logging.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"

namespace gl {

ScopedEGLImage MakeScopedEGLImage(EGLContext context,
                                  EGLenum target,
                                  EGLClientBuffer buffer,
                                  const EGLint* attrs) {
  EGLImageKHR egl_image =
      eglCreateImageKHR(GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(), context,
                        target, buffer, attrs);

  if (egl_image == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "Failed to create EGLImage: " << ui::GetLastEGLErrorString();
  }

  return ScopedEGLImage(egl_image);
}

EGLImageKHR DeleteEGLImageTraits::InvalidValue() {
  return EGL_NO_IMAGE_KHR;
}

void DeleteEGLImageTraits::Free(EGLImageKHR image) {
  // ScopedGeneric guarantees that it will not call Free() in the case where the
  // object held is equal to InvalidValue().
  DCHECK_NE(image, EGL_NO_IMAGE_KHR);

  const EGLBoolean result =
      eglDestroyImageKHR(GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(), image);
  if (result == EGL_FALSE)
    LOG(ERROR) << "Error destroying EGLImage: " << ui::GetLastEGLErrorString();
}

}  // namespace gl
