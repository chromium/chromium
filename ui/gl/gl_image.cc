// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image.h"

#include "base/notreached.h"
#include "build/build_config.h"
#include "ui/gl/gl_bindings.h"

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
GLImageMemory* GLImage::ToGLImageMemory(GLImage* image) {
  if (!image || image->GetType() != Type::MEMORY)
    return nullptr;
  return reinterpret_cast<GLImageMemory*>(image);
}

// static
GLImageIOSurface* GLImage::ToGLImageIOSurface(GLImage* image) {
  if (!image || image->GetType() != Type::IOSURFACE)
    return nullptr;
  return reinterpret_cast<GLImageIOSurface*>(image);
}

// static
GLImageDXGI* GLImage::ToGLImageDXGI(GLImage* image) {
  if (!image || image->GetType() != Type::DXGI_IMAGE)
    return nullptr;
  return reinterpret_cast<GLImageDXGI*>(image);
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

unsigned GLImage::GetInternalFormat() {
  NOTREACHED();
  return GL_NONE;
}

unsigned GLImage::GetDataFormat() {
  // GetInternalFormat() mostly returns unsized format and can be used both
  // as internal format and data format. However, GL_EXT_texture_norm16
  // follows ES3 semantics and only exposes a sized internalformat.
  unsigned internalformat = GetInternalFormat();
  switch (internalformat) {
    case GL_R16_EXT:
      return GL_RED_EXT;
    case GL_RG16_EXT:
      return GL_RG_EXT;
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

unsigned GLImage::GetDataType() {
  NOTREACHED();
  return GL_NONE;
}

GLImage::BindOrCopy GLImage::ShouldBindOrCopy() {
  NOTREACHED();
  return BIND;
}

bool GLImage::BindTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

void GLImage::ReleaseTexImage(unsigned target) {
  NOTREACHED();
}

bool GLImage::CopyTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

bool GLImage::CopyTexSubImage(unsigned target,
                              const gfx::Point& offset,
                              const gfx::Rect& rect) {
  NOTREACHED();
  return false;
}

void GLImage::SetColorSpace(const gfx::ColorSpace& color_space) {
  color_space_ = color_space;
}

void GLImage::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                           uint64_t process_tracing_id,
                           const std::string& dump_name) {
  NOTREACHED();
}

GLImage::Type GLImage::GetType() const {
  return Type::NONE;
}

scoped_refptr<gfx::NativePixmap> GLImage::GetNativePixmap() {
  return nullptr;
}

void* GLImage::GetEGLImage() const {
  return nullptr;
}

}  // namespace gl
