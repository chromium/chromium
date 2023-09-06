// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/scoped_restore_texture.h"

#include "base/feature_list.h"
#include "base/notreached.h"

namespace gl {

namespace {
// The `restore_prev_even_if_invalid` param was introduced to support legacy
// behavior while porting clients of duplicate solutions. Our suspicion is that
// it's not necessary to guard restoring the previous binding with a check that
// it's valid; that behavior was introduced as a speculative fix for a crash
// in https://chromium-review.googlesource.com/c/chromium/src/+/3669775 but
// didn't seem to help
// (https://bugs.chromium.org/p/chromium/issues/detail?id=1327231#c13). This
// killswitch guards switching to unconditionally restore the previous binding.
// Once this change definitively lands, we will unconditionally restore the
// previous binding and remove the `restore_prev_even_if_invalid` parameter
// altogether.
BASE_FEATURE(kAlwaysRestorePrevEvenIfInvalid,
             "AlwaysRestorePrevEvenIfInvalid",
             base::FEATURE_ENABLED_BY_DEFAULT);
}  // namespace

ScopedRestoreTexture::ScopedRestoreTexture(
    gl::GLApi* api,
    GLenum target,
    bool restore_prev_even_if_invalid /*=false*/,
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
      NOTREACHED();
  }
  GLint binding = 0;
  api->glGetIntegervFn(get_target, &binding);
  // The bound texture could be already deleted by another context, and the
  // texture ID |binding| could be reused and points to a different texture.
  // NOTE: See comment on killswitch above.
  if (api->glIsTextureFn(binding) || restore_prev_even_if_invalid ||
      base::FeatureList::IsEnabled(kAlwaysRestorePrevEvenIfInvalid)) {
    prev_binding_ = binding;
  }
  if (new_binding) {
    api->glBindTextureFn(target, new_binding);
  }
}

ScopedRestoreTexture::~ScopedRestoreTexture() {
  api_->glBindTextureFn(target_, prev_binding_);
}

}  // namespace gl
