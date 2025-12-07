// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_SCOPED_GL_TEXTURE_H_
#define UI_GL_SCOPED_GL_TEXTURE_H_

#include "base/scoped_generic.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_export.h"

namespace gl {

struct GL_EXPORT DeleteGLTextureTraits {
  static GLuint InvalidValue();
  static void Free(GLuint texture);
};

// A RAII wrapper for a texture ID that automatically deletes the
// texture when the object goes out of scope. The deletion happens on the
// current GL context.
using ScopedGLTexture = base::ScopedGeneric<GLuint, DeleteGLTextureTraits>;

}  // namespace gl

#endif  // UI_GL_SCOPED_GL_TEXTURE_H_
