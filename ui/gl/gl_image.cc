// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image.h"

#include "ui/gl/gl_bindings.h"

#if defined(OS_ANDROID)
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#endif

namespace gl {

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

bool GLImage::BindTexImageWithInternalformat(unsigned target,
                                             unsigned internalformat) {
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

bool GLImage::ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                                   int z_order,
                                   gfx::OverlayTransform transform,
                                   const gfx::Rect& bounds_rect,
                                   const gfx::RectF& crop_rect,
                                   bool enable_blend,
                                   std::unique_ptr<gfx::GpuFence> gpu_fence) {
  NOTREACHED();
  return false;
}

void GLImage::SetColorSpace(const gfx::ColorSpace& color_space) {
  color_space_ = color_space;
}

void GLImage::Flush() {
  NOTREACHED();
}

void GLImage::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                           uint64_t process_tracing_id,
                           const std::string& dump_name) {
  NOTREACHED();
}

bool GLImage::EmulatingRGB() const {
  return false;
}

bool GLImage::IsInUseByWindowServer() const {
  return false;
}

void GLImage::DisableInUseByWindowServer() {
  NOTIMPLEMENTED();
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

scoped_refptr<gfx::NativePixmap> GLImage::GetNativePixmap() {
  return nullptr;
}

}  // namespace gl
