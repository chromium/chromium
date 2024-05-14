// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_restore_texture.h"

#include "base/notreached.h"

namespace gl {

ScopedRestoreTexture::ScopedRestoreTexture(
    gl::GLApi* api,
    GLenum target,
    GLuint new_binding /*= 0*/)
    : api_(api), target_(target) {
  DCHECK(target == GL_TEXTURE_2D || target == GL_TEXTURE_EXTERNAL_OES ||
         target == GL_TEXTURE_RECTANGLE_ARB);
  GLenum get_target = GL_TEXTURE_BINDING_2D;
  switch (target) {
    case GL_TEXTURE_2D:
      get_target = GL_TEXTURE_BINDING_2D;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      get_target = GL_TEXTURE_BINDING_RECTANGLE_ARB;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      get_target = GL_TEXTURE_BINDING_EXTERNAL_OES;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  GLint binding = 0;
  api->glGetIntegervFn(get_target, &binding);
  prev_binding_ = binding;
  if (new_binding) {
    api->glBindTextureFn(target, new_binding);
  }
}

ScopedRestoreTexture::~ScopedRestoreTexture() {
  api_->glBindTextureFn(target_, prev_binding_);
}

}  // namespace gl
