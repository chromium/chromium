// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_GL_FRAMEBUFFER_H_
#define UI_GL_SCOPED_GL_FRAMEBUFFER_H_

#include "base/scoped_generic.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace gl {

struct GL_EXPORT DeleteGLFramebufferTraits {
  static GLuint InvalidValue();
  static void Free(GLuint framebuffer);
};

// A RAII wrapper for a framebuffer ID that automatically deletes the
// framebuffer when the object goes out of scope. The deletion happens on the
// current GL context.
using ScopedGLFramebuffer =
    base::ScopedGeneric<GLuint, DeleteGLFramebufferTraits>;

// Helper to create a ScopedGLFramebuffer.
GL_EXPORT ScopedGLFramebuffer CreateScopedGLFramebuffer(GLApi* api);

}  // namespace gl

#endif  // UI_GL_SCOPED_GL_FRAMEBUFFER_H_
