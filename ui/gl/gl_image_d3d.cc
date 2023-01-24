// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_d3d.h"

#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"

#ifndef EGL_ANGLE_image_d3d11_texture
#define EGL_D3D11_TEXTURE_ANGLE 0x3484
#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#define EGL_D3D11_TEXTURE_PLANE_ANGLE 0x3492
#define EGL_D3D11_TEXTURE_ARRAY_SLICE_ANGLE 0x3493
#endif /* EGL_ANGLE_image_d3d11_texture */

namespace gl {

GLImageD3D::GLImageD3D(const gfx::Size& size,
                       unsigned internal_format,
                       Microsoft::WRL::ComPtr<ID3D11Texture2D> texture,
                       size_t array_slice,
                       size_t plane_index,
                       Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain)
    : GLImage(),
      size_(size),
      internal_format_(internal_format),
      texture_(std::move(texture)),
      array_slice_(array_slice),
      plane_index_(plane_index),
      swap_chain_(std::move(swap_chain)) {
  DCHECK(texture_);
}

GLImageD3D::~GLImageD3D() {
  if (egl_image_ != EGL_NO_IMAGE_KHR) {
    if (eglDestroyImageKHR(GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(),
                           egl_image_) == EGL_FALSE) {
      DLOG(ERROR) << "Error destroying EGLImage: "
                  << ui::GetLastEGLErrorString();
    }
  }
}

bool GLImageD3D::Initialize() {
  DCHECK_EQ(egl_image_, EGL_NO_IMAGE_KHR);
  const EGLint attribs[] = {EGL_TEXTURE_INTERNAL_FORMAT_ANGLE,
                            static_cast<EGLint>(internal_format_),
                            EGL_D3D11_TEXTURE_ARRAY_SLICE_ANGLE,
                            static_cast<EGLint>(array_slice_),
                            EGL_D3D11_TEXTURE_PLANE_ANGLE,
                            static_cast<EGLint>(plane_index_),
                            EGL_NONE};
  egl_image_ =
      eglCreateImageKHR(GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(),
                        EGL_NO_CONTEXT, EGL_D3D11_TEXTURE_ANGLE,
                        static_cast<EGLClientBuffer>(texture_.Get()), attribs);
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "Error creating EGLImage: " << ui::GetLastEGLErrorString();
    return false;
  }
  return true;
}

GLImage::Type GLImageD3D::GetType() const {
  return Type::D3D;
}

void* GLImageD3D::GetEGLImage() const {
  return egl_image_;
}

gfx::Size GLImageD3D::GetSize() {
  return size_;
}

bool GLImageD3D::BindTexImage(unsigned target) {
  DCHECK_NE(egl_image_, EGL_NO_IMAGE_KHR);
  glEGLImageTargetTexture2DOES(target, egl_image_);
  return glGetError() == static_cast<GLenum>(GL_NO_ERROR);
}

}  // namespace gl
