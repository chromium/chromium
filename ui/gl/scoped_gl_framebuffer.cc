// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_gl_framebuffer.h"

#include "base/check.h"
#include "ui/gl/gl_context.h"

namespace gl {

// static
GLuint DeleteGLFramebufferTraits::InvalidValue() {
  return 0;
}

// static
void DeleteGLFramebufferTraits::Free(GLuint framebuffer) {
  if (!framebuffer) {
    return;
  }
  // A GL context must be current to delete a framebuffer.
  DCHECK(GLContext::GetCurrent());
  GLApi* api = g_current_gl_context;
  api->glDeleteFramebuffersEXTFn(1, &framebuffer);
}

ScopedGLFramebuffer CreateScopedGLFramebuffer(GLApi* api) {
  GLuint framebuffer = 0;
  api->glGenFramebuffersEXTFn(1, &framebuffer);
  return ScopedGLFramebuffer(framebuffer);
}

}  // namespace gl
