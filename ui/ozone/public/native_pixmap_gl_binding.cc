// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/native_pixmap_gl_binding.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/scoped_binders.h"

namespace ui {

NativePixmapGLBinding::NativePixmapGLBinding() = default;
NativePixmapGLBinding::~NativePixmapGLBinding() = default;

// The GLImgeNativePixmap::BindTexImage and GLImageNativePixmap::Initialize will
// be merged to NativePixmapEGLBinding and corresponding code for
// GLImageGLXNativePixmap will move to NativePixmapGLXBinding leading to the
// deletion of BindTexture here.
bool NativePixmapGLBinding::BindTexture(scoped_refptr<gl::GLImage> gl_image,
                                        GLenum target,
                                        GLuint texture_id) {
  gl_image_ = gl_image;
  gl::ScopedTextureBinder binder(target, texture_id);

  gl::GLApi* api = gl::g_current_gl_context;
  DCHECK(api);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  if (!gl_image_->BindTexImage(target)) {
    LOG(ERROR) << "Unable to bind GL image to target = " << target;
    return false;
  }

  return true;
}

GLuint NativePixmapGLBinding::GetInternalFormat() {
  return gl_image_->GetInternalFormat();
}

GLenum NativePixmapGLBinding::GetDataFormat() {
  return gl_image_->GetDataFormat();
}

GLenum NativePixmapGLBinding::GetDataType() {
  return gl_image_->GetDataType();
}

}  // namespace ui
