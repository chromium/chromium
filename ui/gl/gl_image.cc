// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image.h"

#include <GL/gl.h>

#include "base/notreached.h"

namespace gl {

// NOTE: It is not possible to use static_cast in the below "safe downcast"
// functions, as the compiler doesn't know that the various GLImage subclasses
// do in fact inherit from GLImage. However, the reinterpret_casts used are
// safe, as |image| actually is an instance of the type in question.

// static
GLImageD3D* GLImage::ToGLImageD3D(GLImage* image) {
  if (!image || image->GetType() != Type::D3D)
    return nullptr;
  return reinterpret_cast<GLImageD3D*>(image);
}

// static
// static
media::GLImageEGLStream* GLImage::ToGLImageEGLStream(GLImage* image) {
  if (!image || image->GetType() != Type::EGL_STREAM) {
    return nullptr;
  }
  return reinterpret_cast<media::GLImageEGLStream*>(image);
}

// static
media::GLImagePbuffer* GLImage::ToGLImagePbuffer(GLImage* image) {
  if (!image || image->GetType() != Type::PBUFFER)
    return nullptr;
  return reinterpret_cast<media::GLImagePbuffer*>(image);
}

gfx::Size GLImage::GetSize() {
  NOTREACHED();
  return gfx::Size();
}

GLImage::Type GLImage::GetType() const {
  return Type::NONE;
}

}  // namespace gl
