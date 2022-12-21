// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_EGL_IMAGE_H_
#define UI_GL_SCOPED_EGL_IMAGE_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "base/scoped_generic.h"
#include "ui/gl/gl_export.h"

namespace gl {

struct GL_EXPORT DeleteEGLImageTraits {
  static EGLImageKHR InvalidValue();
  static void Free(EGLImageKHR image);
};
using ScopedEGLImage = base::ScopedGeneric<EGLImageKHR, DeleteEGLImageTraits>;

// Creates a ScopedEGLImage holding an EGLImage that was created from the
// passed-in arguments (unless the EGLImage wasn't able to be
// successfully created, in which case the returned object holds null).
// NOTE: The GLDisplay used to create and destroy the image will be
// gl::GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay().
// TODO(blundell): Generalize to pass in the GLDisplay and fold
// //components/exo's ScopedEglImage into this one.
GL_EXPORT ScopedEGLImage MakeScopedEGLImage(EGLContext context,
                                            EGLenum target,
                                            EGLClientBuffer buffer,
                                            const EGLint* attrs);

}  // namespace gl

#endif  // UI_GL_SCOPED_EGL_IMAGE_H_
