// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_io_surface_egl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/scoped_binders.h"

// Enums for the EGL_ANGLE_iosurface_client_buffer extension
#define EGL_IOSURFACE_ANGLE 0x3454
#define EGL_IOSURFACE_PLANE_ANGLE 0x345A
#define EGL_TEXTURE_RECTANGLE_ANGLE 0x345B
#define EGL_TEXTURE_TYPE_ANGLE 0x345C
#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#define EGL_BIND_TO_TEXTURE_TARGET_ANGLE 0x348D

namespace gl {

namespace {

// If enabled, this will release all EGL state as soon as the underlying
// texture is released. This has the potential to cause performance regressions,
// and so is disabled by default.
BASE_FEATURE(kTightlyScopedIOSurfaceEGLState,
             "TightlyScopedIOSurfaceEGLState",
             base::FEATURE_DISABLED_BY_DEFAULT);

struct InternalFormatType {
  InternalFormatType(GLenum format, GLenum type) : format(format), type(type) {}

  GLenum format;
  GLenum type;
};

// Convert a gfx::BufferFormat to a (internal format, type) combination from the
// EGL_ANGLE_iosurface_client_buffer extension spec.
InternalFormatType BufferFormatToInternalFormatType(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return {GL_RED, GL_UNSIGNED_BYTE};
    case gfx::BufferFormat::R_16:
      return {GL_RED, GL_UNSIGNED_SHORT};
    case gfx::BufferFormat::RG_88:
      return {GL_RG, GL_UNSIGNED_BYTE};
    case gfx::BufferFormat::RG_1616:
      return {GL_RG, GL_UNSIGNED_SHORT};
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBX_8888:
      return {GL_RGB, GL_UNSIGNED_BYTE};
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::RGBA_8888:
      return {GL_BGRA_EXT, GL_UNSIGNED_BYTE};
    case gfx::BufferFormat::RGBA_F16:
      return {GL_RGBA, GL_HALF_FLOAT};
    case gfx::BufferFormat::BGRA_1010102:
      return {GL_RGB10_A2, GL_UNSIGNED_INT_2_10_10_10_REV};
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_1010102:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::P010:
      NOTREACHED();
      return {GL_NONE, GL_NONE};
  }

  NOTREACHED();
  return {GL_NONE, GL_NONE};
}

GLint GLTargetFromEGLTarget(GLint egl_target) {
  switch (egl_target) {
    case EGL_TEXTURE_2D:
      return GL_TEXTURE_2D;
    case EGL_TEXTURE_RECTANGLE_ANGLE:
      return GL_TEXTURE_RECTANGLE_ARB;
    default:
      NOTIMPLEMENTED() << " Target not supported.";
      return GL_NONE;
  }
}

EGLint EGLTargetFromGLTarget(GLint gl_target) {
  switch (gl_target) {
    case GL_TEXTURE_2D:
      return EGL_TEXTURE_2D;
    case GL_TEXTURE_RECTANGLE_ARB:
      return EGL_TEXTURE_RECTANGLE_ANGLE;
    default:
      NOTIMPLEMENTED() << " Target not supported.";
      return EGL_NO_TEXTURE;
  }
}

GLenum TargetGetterFromGLTarget(GLint gl_target) {
  switch (gl_target) {
    case GL_TEXTURE_2D:
      return GL_TEXTURE_BINDING_2D;
    case GL_TEXTURE_CUBE_MAP:
      return GL_TEXTURE_BINDING_CUBE_MAP;
    case GL_TEXTURE_EXTERNAL_OES:
      return GL_TEXTURE_BINDING_EXTERNAL_OES;
    case GL_TEXTURE_RECTANGLE_ARB:
      return GL_TEXTURE_BINDING_RECTANGLE_ARB;
    default:
      NOTIMPLEMENTED() << " Target not supported.";
      return GL_NONE;
  }
}

}  // anonymous namespace

EGLAccess::EGLAccess(GLDisplayEGL* display) {
  display_ = display;
  DCHECK_NE(display_, nullptr);
  DCHECK_NE(display_->GetDisplay(), EGL_NO_DISPLAY);

  // When creating a pbuffer we need to supply an EGLConfig. On ANGLE and
  // Swiftshader on Mac, there's only ever one config. Query it from EGL.
  EGLint numConfigs = 0;
  EGLBoolean result = eglChooseConfig(display_->GetDisplay(), nullptr,
                                      &dummy_config_, 1, &numConfigs);
  DCHECK_EQ(result, static_cast<EGLBoolean>(EGL_TRUE));
  DCHECK_EQ(numConfigs, 1);
  DCHECK_NE(dummy_config_, EGL_NO_CONFIG_KHR);

  // If EGL_BIND_TO_TEXTURE_TARGET_ANGLE has already been queried, then don't
  // query it again, since that depends only on the ANGLE backend (and we will
  // not be mixing backends in a single process).
  if (texture_target_ == EGL_NO_TEXTURE) {
    texture_target_ = EGL_TEXTURE_RECTANGLE_ANGLE;
    if (display_->ext->b_EGL_ANGLE_iosurface_client_buffer) {
      result = eglGetConfigAttrib(display_->GetDisplay(), dummy_config_,
                                  EGL_BIND_TO_TEXTURE_TARGET_ANGLE,
                                  &texture_target_);
      DCHECK_EQ(result, static_cast<EGLBoolean>(EGL_TRUE));
    }
  }
  DCHECK_NE(texture_target_, EGL_NO_TEXTURE);
}

EGLAccess::~EGLAccess() {
  if (pbuffer_ != EGL_NO_SURFACE) {
    EGLBoolean result = eglDestroySurface(display_->GetDisplay(), pbuffer_);
    DCHECK_EQ(result, static_cast<EGLBoolean>(EGL_TRUE));
  }
}

GLImageIOSurfaceEGL::GLImageIOSurfaceEGL(const gfx::Size& size,
                                         unsigned internalformat)
    : GLImageIOSurface(size, internalformat) {}

GLImageIOSurfaceEGL::~GLImageIOSurfaceEGL() {
  if (texture_bound_) {
    GLint target_gl =
        GLTargetFromEGLTarget(GetEGLAccessForCurrentContext().texture_target());
    DCHECK_NE(target_gl, GL_NONE);
    ReleaseTexImage(target_gl);
  }
}

EGLAccess& GLImageIOSurfaceEGL::GetEGLAccessForCurrentContext() {
  GLDisplayEGL* display = GLDisplayEGL::GetDisplayForCurrentContext();
  DCHECK_NE(display, nullptr);
  if (!egl_access_map_.contains(display)) {
    egl_access_map_.emplace(display, EGLAccess(display));
  }
  return egl_access_map_.at(display);
}

void GLImageIOSurfaceEGL::ReleaseTexImage(unsigned target) {
  EGLint target_egl = EGLTargetFromGLTarget(target);
  if (target_egl == EGL_NO_TEXTURE) {
    return;
  }

  if (!texture_bound_) {
    return;
  }

  EGLAccess& egl_access = GetEGLAccessForCurrentContext();

  DCHECK(egl_access.texture_target() == target_egl);
  DCHECK_NE(egl_access.pbuffer(), EGL_NO_SURFACE);

  EGLBoolean result = eglReleaseTexImage(egl_access.display()->GetDisplay(),
                                         egl_access.pbuffer(), EGL_BACK_BUFFER);
  DCHECK_EQ(result, static_cast<EGLBoolean>(EGL_TRUE));
  texture_bound_ = false;

  if (base::FeatureList::IsEnabled(kTightlyScopedIOSurfaceEGLState)) {
    result = eglDestroySurface(egl_access.display()->GetDisplay(),
                               egl_access.pbuffer());
    DCHECK_EQ(result, static_cast<EGLBoolean>(EGL_TRUE));
    egl_access.set_pbuffer(EGL_NO_SURFACE);
  }
}

bool GLImageIOSurfaceEGL::BindTexImageImpl(unsigned target,
                                           unsigned internalformat) {
  // TODO(cwallez@chromium.org): internalformat is used by Blink's
  // DrawingBuffer::SetupRGBEmulationForBlitFramebuffer to bind an RGBA
  // IOSurface as RGB. We should support this.

  CHECK(!texture_bound_) << "Cannot re-bind already bound IOSurface.";

  GLenum target_getter = TargetGetterFromGLTarget(target);
  EGLint target_egl = EGLTargetFromGLTarget(target);
  if (target_getter == GL_NONE || target_egl == EGL_NO_TEXTURE) {
    return false;
  }

  EGLAccess& egl_access = GetEGLAccessForCurrentContext();

  DCHECK_EQ(egl_access.texture_target(), target_egl);

  if (internalformat != 0) {
    LOG(ERROR) << "GLImageIOSurfaceEGL doesn't support binding with a custom "
                  "internal format yet.";
    return false;
  }

  // Create the pbuffer representing this IOSurface lazily because we don't know
  // in the constructor if we're going to be used to bind plane 0 to a texture,
  // or to transform YUV to RGB.
  if (egl_access.pbuffer() == EGL_NO_SURFACE) {
    InternalFormatType formatType = BufferFormatToInternalFormatType(format_);

    // clang-format off
    const EGLint attribs[] = {
      EGL_WIDTH,                         size_.width(),
      EGL_HEIGHT,                        size_.height(),
      EGL_IOSURFACE_PLANE_ANGLE,         static_cast<EGLint>(io_surface_plane_),
      EGL_TEXTURE_TARGET,                egl_access.texture_target(),
      EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, static_cast<EGLint>(formatType.format),
      EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
      EGL_TEXTURE_TYPE_ANGLE,            static_cast<EGLint>(formatType.type),
      EGL_NONE,                          EGL_NONE,
    };
    // clang-format on

    egl_access.set_pbuffer(eglCreatePbufferFromClientBuffer(
        egl_access.display()->GetDisplay(), EGL_IOSURFACE_ANGLE,
        io_surface_.get(), egl_access.dummy_config(), attribs));
    if (egl_access.pbuffer() == EGL_NO_SURFACE) {
      LOG(ERROR) << "eglCreatePbufferFromClientBuffer failed, EGL error is "
                 << eglGetError();
      return false;
    }
  }

  DCHECK(!texture_bound_);
  EGLBoolean result = eglBindTexImage(egl_access.display()->GetDisplay(),
                                      egl_access.pbuffer(), EGL_BACK_BUFFER);

  if (result != EGL_TRUE) {
    LOG(ERROR) << "eglBindTexImage failed, EGL error is "
               << eglGetError();
    eglDestroySurface(egl_access.display()->GetDisplay(), egl_access.pbuffer());
    egl_access.set_pbuffer(EGL_NO_SURFACE);
    return false;
  }

  texture_bound_ = true;
  return true;
}

}  // namespace gl
