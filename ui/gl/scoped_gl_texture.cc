// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_gl_texture.h"

#include "base/check.h"
#include "ui/gl/gl_context.h"

namespace gl {

// static
GLuint DeleteGLTextureTraits::InvalidValue() {
  return 0;
}

// static
void DeleteGLTextureTraits::Free(GLuint texture) {
  if (!texture) {
    return;
  }
  // A GL context must be current to delete a texture.
  DCHECK(GLContext::GetCurrent());
  GLApi* api = g_current_gl_context;
  api->glDeleteTexturesFn(1, &texture);
}

}  // namespace gl
