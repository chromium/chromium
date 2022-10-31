// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_ahardwarebuffer.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_utils.h"

namespace gl {
namespace {

uint32_t GetBufferFormat(const AHardwareBuffer* buffer) {
  AHardwareBuffer_Desc desc = {};
  base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer, &desc);
  return desc.format;
}

unsigned int GLInternalFormat(uint32_t buffer_format) {
  switch (buffer_format) {
    case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
    case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
    case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
      return GL_RGBA;
    case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
    case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
    case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
      return GL_RGB;
    default:
      // For all other buffer formats, use GL_RGBA as internal format.
      return GL_RGBA;
  }
}

unsigned int GLDataType(uint32_t buffer_format) {
  switch (buffer_format) {
    case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
    case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
    case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
      return GL_UNSIGNED_BYTE;
    case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
      return GL_HALF_FLOAT_OES;
    case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
      return GL_UNSIGNED_INT_2_10_10_10_REV;
    case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
      return GL_UNSIGNED_SHORT_5_6_5;
    default:
      // For all other buffer formats, use GL_UNSIGNED_BYTE as type.
      return GL_UNSIGNED_BYTE;
  }
}

}  // namespace

GLImageAHardwareBuffer::GLImageAHardwareBuffer(const gfx::Size& size)
    : GLImageEGL(size) {}

GLImageAHardwareBuffer::~GLImageAHardwareBuffer() {}

bool GLImageAHardwareBuffer::Initialize(AHardwareBuffer* buffer,
                                        bool preserved) {
  handle_ = base::android::ScopedHardwareBufferHandle::Create(buffer);
  uint32_t buffer_format = GetBufferFormat(buffer);
  internal_format_ = GLInternalFormat(buffer_format);
  data_type_ = GLDataType(buffer_format);
  EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, preserved ? EGL_TRUE : EGL_FALSE,
                      EGL_NONE};
  EGLClientBuffer client_buffer = eglGetNativeClientBufferANDROID(buffer);
  return GLImageEGL::Initialize(EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                                client_buffer, attribs);
}

unsigned GLImageAHardwareBuffer::GetInternalFormat() {
  return internal_format_;
}

unsigned GLImageAHardwareBuffer::GetDataType() {
  return data_type_;
}

bool GLImageAHardwareBuffer::BindTexImage(unsigned target) {
  return GLImageEGL::BindTexImage(target);
}

bool GLImageAHardwareBuffer::CopyTexImage(unsigned target) {
  return false;
}

bool GLImageAHardwareBuffer::CopyTexSubImage(unsigned target,
                                             const gfx::Point& offset,
                                             const gfx::Rect& rect) {
  return false;
}

void GLImageAHardwareBuffer::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {}

}  // namespace gl
