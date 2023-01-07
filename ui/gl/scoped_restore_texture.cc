// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_restore_texture.h"

namespace gl {

ScopedRestoreTexture::ScopedRestoreTexture(gl::GLApi* api, GLenum target)
    : api_(api), target_(target) {
  DCHECK(target == GL_TEXTURE_2D || target == GL_TEXTURE_EXTERNAL_OES);
  GLint binding = 0;
  api->glGetIntegervFn(target == GL_TEXTURE_2D
                           ? GL_TEXTURE_BINDING_2D
                           : GL_TEXTURE_BINDING_EXTERNAL_OES,
                       &binding);
  // The bound texture could be already deleted by another context, and the
  // texture ID |binding| could be reused and points to a different texture.
  if (api->glIsTextureFn(binding))
    prev_binding_ = binding;
}

ScopedRestoreTexture::~ScopedRestoreTexture() {
  api_->glBindTextureFn(target_, prev_binding_);
}

}  // namespace gl
