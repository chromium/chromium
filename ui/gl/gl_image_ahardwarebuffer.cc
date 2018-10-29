// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_ahardwarebuffer.h"

#include "ui/gl/gl_bindings.h"

namespace gl {

GLImageAHardwareBuffer::GLImageAHardwareBuffer(const gfx::Size& size)
    : GLImageEGL(size) {}

GLImageAHardwareBuffer::~GLImageAHardwareBuffer() {}

bool GLImageAHardwareBuffer::Initialize(AHardwareBuffer* buffer,
                                        bool preserved) {
  handle_ = base::android::ScopedHardwareBufferHandle::Create(buffer);
  EGLint attribs[] = {EGL_IMAGE_PRESERVED_KHR, preserved ? EGL_TRUE : EGL_FALSE,
                      EGL_NONE};
  EGLClientBuffer client_buffer = eglGetNativeClientBufferANDROID(buffer);
  return GLImageEGL::Initialize(EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                                client_buffer, attribs);
}

unsigned GLImageAHardwareBuffer::GetInternalFormat() {
  return GL_RGBA;
}

bool GLImageAHardwareBuffer::CopyTexImage(unsigned target) {
  return false;
}

bool GLImageAHardwareBuffer::CopyTexSubImage(unsigned target,
                                             const gfx::Point& offset,
                                             const gfx::Rect& rect) {
  return false;
}

bool GLImageAHardwareBuffer::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  return false;
}

void GLImageAHardwareBuffer::Flush() {}

void GLImageAHardwareBuffer::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {}

std::unique_ptr<GLImage::ScopedHardwareBuffer>
GLImageAHardwareBuffer::GetAHardwareBuffer() {
  return std::make_unique<ScopedHardwareBuffer>(
      base::android::ScopedHardwareBufferHandle::Create(handle_.get()),
      base::ScopedFD());
}

}  // namespace gl
