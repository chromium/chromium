// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/egl_surface_io_surface.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_bindings.h"

// Enums for the EGL_ANGLE_iosurface_client_buffer extension
#define EGL_IOSURFACE_ANGLE 0x3454
#define EGL_IOSURFACE_PLANE_ANGLE 0x345A
#define EGL_TEXTURE_RECTANGLE_ANGLE 0x345B
#define EGL_TEXTURE_TYPE_ANGLE 0x345C
#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#define EGL_BIND_TO_TEXTURE_TARGET_ANGLE 0x348D

using gfx::BufferFormat;

namespace gl {

namespace {

struct InternalFormatType {
  InternalFormatType(GLenum format, GLenum type) : format(format), type(type) {}

  GLenum format;
  GLenum type;
};

// Convert a BufferFormat to a (internal format, type) combination from the
// EGL_ANGLE_iosurface_client_buffer extension spec.
InternalFormatType BufferFormatToInternalFormatType(BufferFormat format) {
  switch (format) {
    case BufferFormat::R_8:
      return {GL_RED, GL_UNSIGNED_BYTE};
    case BufferFormat::R_16:
      return {GL_RED, GL_UNSIGNED_SHORT};
    case BufferFormat::RG_88:
      return {GL_RG, GL_UNSIGNED_BYTE};
    case BufferFormat::RG_1616:
      return {GL_RG, GL_UNSIGNED_SHORT};
    case BufferFormat::BGRX_8888:
    case BufferFormat::RGBX_8888:
      return {GL_RGB, GL_UNSIGNED_BYTE};
    case BufferFormat::BGRA_8888:
      return {GL_BGRA_EXT, GL_UNSIGNED_BYTE};
    case BufferFormat::RGBA_8888:
      return {GL_RGBA, GL_UNSIGNED_BYTE};
    case BufferFormat::RGBA_F16:
      return {GL_RGBA, GL_HALF_FLOAT};
    case BufferFormat::BGRA_1010102:
      return {GL_RGB10_A2, GL_UNSIGNED_INT_2_10_10_10_REV};
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_1010102:
    case BufferFormat::YVU_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::YUVA_420_TRIPLANAR:
    case BufferFormat::P010:
      break;
      return {GL_NONE, GL_NONE};
  }

  LOG(ERROR) << "Invalid format: " << BufferFormatToString(format);
  return {GL_NONE, GL_NONE};
}

}  // anonymous namespace

////////////////////////////////////////////////////////////////////////////////
// ScopedEGLSurfaceIOSurface

// static
std::unique_ptr<ScopedEGLSurfaceIOSurface> ScopedEGLSurfaceIOSurface::Create(
    EGLDisplay display,
    unsigned target,
    IOSurfaceRef io_surface,
    uint32_t plane,
    gfx::BufferFormat format) {
  if (display == EGL_NO_DISPLAY) {
    LOG(ERROR) << "Invalid GLDisplayEGL.";
    return nullptr;
  }

  std::unique_ptr<ScopedEGLSurfaceIOSurface> result(
      new ScopedEGLSurfaceIOSurface(display));

  if (!result->ValidateTarget(target)) {
    return nullptr;
  }

  if (!result->CreatePBuffer(io_surface, plane, format)) {
    LOG(ERROR) << "Failed to create PBuffer for IOSurface.";
    return nullptr;
  }

  return result;
}

ScopedEGLSurfaceIOSurface::ScopedEGLSurfaceIOSurface(EGLDisplay display)
    : display_(display) {
  // When creating a pbuffer we need to supply an EGLConfig. On ANGLE and
  // Swiftshader on Mac, there's only ever one config. Query it from EGL.
  EGLint numConfigs = 0;
  EGLBoolean result =
      eglChooseConfig(display_, nullptr, &dummy_config_, 1, &numConfigs);
  DCHECK_EQ(result, static_cast<EGLBoolean>(EGL_TRUE));
  DCHECK_EQ(numConfigs, 1);
  DCHECK_NE(dummy_config_, EGL_NO_CONFIG_KHR);

  // If EGL_BIND_TO_TEXTURE_TARGET_ANGLE has already been queried, then don't
  // query it again, since that depends only on the ANGLE backend (and we will
  // not be mixing backends in a single process).
  if (texture_target_ == EGL_NO_TEXTURE) {
    texture_target_ = EGL_TEXTURE_RECTANGLE_ANGLE;
    eglGetConfigAttrib(display_, dummy_config_,
                       EGL_BIND_TO_TEXTURE_TARGET_ANGLE, &texture_target_);
  }
  DCHECK_NE(texture_target_, EGL_NO_TEXTURE);
}

bool ScopedEGLSurfaceIOSurface::ValidateTarget(unsigned target) const {
  switch (target) {
    case GL_TEXTURE_2D:
      if (texture_target_ != EGL_TEXTURE_2D) {
        LOG(ERROR) << "eglBindTexImage requires 2D, got: " << target;
        return false;
      }
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      if (texture_target_ != EGL_TEXTURE_RECTANGLE_ANGLE) {
        LOG(ERROR) << "eglBindTexImage requires RECTANGLE, got: " << target;
        return false;
      }
      break;
    default:
      LOG(ERROR) << "Invalid texture target: " << target;
      return false;
  }
  return true;
}

bool ScopedEGLSurfaceIOSurface::CreatePBuffer(IOSurfaceRef io_surface,
                                              uint32_t plane,
                                              gfx::BufferFormat format) {
  // Create the pbuffer representing this IOSurface lazily because we don't know
  // in the constructor if we're going to be used to bind plane 0 to a texture,
  // or to transform YUV to RGB.
  if (pbuffer_ == EGL_NO_SURFACE) {
    InternalFormatType formatType = BufferFormatToInternalFormatType(format);
    if (formatType.format == GL_NONE || formatType.type == GL_NONE) {
      LOG(ERROR) << "Invalid resource format.";
      return false;
    }
    EGLint width =
        static_cast<EGLint>(IOSurfaceGetWidthOfPlane(io_surface, plane));
    EGLint height =
        static_cast<EGLint>(IOSurfaceGetHeightOfPlane(io_surface, plane));

    // clang-format off
    const EGLint attribs[] = {
      EGL_WIDTH,                         width,
      EGL_HEIGHT,                        height,
      EGL_IOSURFACE_PLANE_ANGLE,         static_cast<EGLint>(plane),
      EGL_TEXTURE_TARGET,                texture_target_,
      EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, static_cast<EGLint>(formatType.format),
      EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
      EGL_TEXTURE_TYPE_ANGLE,            static_cast<EGLint>(formatType.type),
      EGL_NONE,                          EGL_NONE,
    };
    // clang-format on

    pbuffer_ = eglCreatePbufferFromClientBuffer(
        display_, EGL_IOSURFACE_ANGLE, io_surface, dummy_config_, attribs);
    if (pbuffer_ == EGL_NO_SURFACE) {
      LOG(ERROR) << "eglCreatePbufferFromClientBuffer failed, EGL error is "
                 << eglGetError();
      return false;
    }
  }

  return true;
}

bool ScopedEGLSurfaceIOSurface::BindTexImage() {
  CHECK(!texture_bound_);
  EGLBoolean result = eglBindTexImage(display_, pbuffer_, EGL_BACK_BUFFER);
  if (result != EGL_TRUE) {
    LOG(ERROR) << "eglBindTexImage failed, EGL error is " << eglGetError();
    return false;
  }

  texture_bound_ = true;
  return true;
}

void ScopedEGLSurfaceIOSurface::ReleaseTexImage() {
  if (!texture_bound_)
    return;

  EGLBoolean result = eglReleaseTexImage(display_, pbuffer_, EGL_BACK_BUFFER);
  DCHECK_EQ(result, static_cast<EGLBoolean>(EGL_TRUE));
  texture_bound_ = false;
}

void ScopedEGLSurfaceIOSurface::DestroyPBuffer() {
  if (pbuffer_ == EGL_NO_SURFACE)
    return;

  EGLBoolean result = eglDestroySurface(display_, pbuffer_);
  DCHECK_EQ(result, static_cast<EGLBoolean>(EGL_TRUE));
  pbuffer_ = EGL_NO_SURFACE;
}

ScopedEGLSurfaceIOSurface::~ScopedEGLSurfaceIOSurface() {
  ReleaseTexImage();
  DestroyPBuffer();
}

}  // namespace gl
