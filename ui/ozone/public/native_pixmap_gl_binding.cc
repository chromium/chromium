// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/native_pixmap_gl_binding.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/scoped_binders.h"

namespace ui {

NativePixmapGLBinding::NativePixmapGLBinding() = default;
NativePixmapGLBinding::~NativePixmapGLBinding() = default;

// static
bool NativePixmapGLBinding::BindTexture(gl::GLImage* gl_image,
                                        GLenum target,
                                        GLuint texture_id) {
  gl::ScopedTextureBinder binder(target, texture_id);

  gl::GLApi* api = gl::g_current_gl_context;
  DCHECK(api);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (!gl_image->BindTexImage(target)) {
    LOG(ERROR) << "Unable to bind GL image to target = " << target;
    return false;
  }

  return true;
}

}  // namespace ui
