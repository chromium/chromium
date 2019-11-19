// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image.h"

#include "ui/gl/gl_bindings.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#endif

namespace gl {

bool GLImage::BindTexImageWithInternalformat(unsigned target,
                                             unsigned internalformat) {
  return false;
}

void GLImage::SetColorSpace(const gfx::ColorSpace& color_space) {
  color_space_ = color_space;
}

bool GLImage::EmulatingRGB() const {
  return false;
}

GLImage::Type GLImage::GetType() const {
  return Type::NONE;
}

#if defined(OS_ANDROID)
std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
GLImage::GetAHardwareBuffer() {
  return nullptr;
}
#endif

bool GLImage::HasMutableState() const {
  return true;
}

unsigned GLImage::GetDataFormat() {
  // GetInternalFormat() mostly returns unsized format and can be used both
  // as internal format and data format. However, GL_EXT_texture_norm16
  // follows ES3 semantics and only exposes a sized internalformat.
  unsigned internalformat = GetInternalFormat();
  switch (internalformat) {
    case GL_R16_EXT:
      return GL_RED;
    case GL_RGB10_A2_EXT:
      return GL_RGBA;
    case GL_RGB_YCRCB_420_CHROMIUM:
    case GL_RGB_YCBCR_420V_CHROMIUM:
    case GL_RGB_YCBCR_P010_CHROMIUM:
      return GL_RGB;
    case GL_RED:
    case GL_RG:
    case GL_RGB:
    case GL_RGBA:
    case GL_BGRA_EXT:
      return internalformat;
    default:
      NOTREACHED();
      return GL_NONE;
  }
}

}  // namespace gl
