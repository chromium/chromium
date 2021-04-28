// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_d3d.h"

#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"

#ifndef EGL_ANGLE_image_d3d11_texture
#define EGL_D3D11_TEXTURE_ANGLE 0x3484
#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#endif /* EGL_ANGLE_image_d3d11_texture */

namespace gl {

namespace {

bool ValidGLInternalFormat(unsigned internal_format) {
  switch (internal_format) {
    case GL_RGB:
    case GL_RGBA:
    case GL_BGRA_EXT:
      return true;
    default:
      return false;
  }
}

bool ValidGLDataType(unsigned data_type) {
  switch (data_type) {
    case GL_UNSIGNED_BYTE:
    case GL_HALF_FLOAT_OES:
      return true;
    default:
      return false;
  }
}

}  // namespace

GLImageD3D::GLImageD3D(const gfx::Size& size,
                       unsigned internal_format,
                       unsigned data_type,
                       Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
                       Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain)
    : GLImage(),
      size_(size),
      internal_format_(internal_format),
      data_type_(data_type),
      texture_(std::move(texture)),
      swap_chain_(std::move(swap_chain)) {
  DCHECK(texture_);
  DCHECK(ValidGLInternalFormat(internal_format_));
  DCHECK(ValidGLDataType(data_type_));
}

GLImageD3D::~GLImageD3D() {
  if (egl_image_ != EGL_NO_IMAGE_KHR) {
    if (eglDestroyImageKHR(GLSurfaceEGL::GetHardwareDisplay(), egl_image_) ==
        EGL_FALSE) {
      DLOG(ERROR) << "Error destroying EGLImage: "
                  << ui::GetLastEGLErrorString();
    }
  }
}

bool GLImageD3D::Initialize() {
  DCHECK_EQ(egl_image_, EGL_NO_IMAGE_KHR);
  const EGLint attribs[] = {EGL_TEXTURE_INTERNAL_FORMAT_ANGLE,
                            GetInternalFormat(), EGL_NONE};
  egl_image_ =
      eglCreateImageKHR(GLSurfaceEGL::GetHardwareDisplay(), EGL_NO_CONTEXT,
                        EGL_D3D11_TEXTURE_ANGLE,
                        static_cast<EGLClientBuffer>(texture_.Get()), attribs);
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "Error creating EGLImage: " << ui::GetLastEGLErrorString();
    return false;
  }
  return true;
}

// static
GLImageD3D* GLImageD3D::FromGLImage(GLImage* image) {
  if (!image || image->GetType() != Type::D3D)
    return nullptr;
  return static_cast<GLImageD3D*>(image);
}

GLImage::Type GLImageD3D::GetType() const {
  return Type::D3D;
}

GLImage::BindOrCopy GLImageD3D::ShouldBindOrCopy() {
  return GLImage::BIND;
}

gfx::Size GLImageD3D::GetSize() {
  return size_;
}

unsigned GLImageD3D::GetInternalFormat() {
  return internal_format_;
}

unsigned GLImageD3D::GetDataType() {
  return data_type_;
}

bool GLImageD3D::BindTexImage(unsigned target) {
  DCHECK_NE(egl_image_, EGL_NO_IMAGE_KHR);
  glEGLImageTargetTexture2DOES(target, egl_image_);
  return glGetError() == static_cast<GLenum>(GL_NO_ERROR);
}

bool GLImageD3D::CopyTexImage(unsigned target) {
  return false;
}

bool GLImageD3D::CopyTexSubImage(unsigned target,
                                 const gfx::Point& offset,
                                 const gfx::Rect& rect) {
  return false;
}

void GLImageD3D::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                              uint64_t process_tracing_id,
                              const std::string& dump_name) {
  NOTIMPLEMENTED_LOG_ONCE();
}

bool GLImageD3D::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  NOTREACHED();
  return false;
}

}  // namespace gl
