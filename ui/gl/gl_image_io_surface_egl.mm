// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_io_surface_egl.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/yuv_to_rgb_converter.h"

// Enums for the EGL_ANGLE_iosurface_client_buffer extension
#define EGL_IOSURFACE_ANGLE 0x3454
#define EGL_IOSURFACE_PLANE_ANGLE 0x345A
#define EGL_TEXTURE_RECTANGLE_ANGLE 0x345B
#define EGL_TEXTURE_TYPE_ANGLE 0x345C
#define EGL_TEXTURE_INTERNAL_FORMAT_ANGLE 0x345D
#define EGL_BIND_TO_TEXTURE_TARGET_ANGLE 0x348D

namespace gl {

namespace {

struct InternalFormatType {
  InternalFormatType(GLenum format, GLenum type) : format(format), type(type) {}

  GLenum format;
  GLenum type;
};

// Convert a gfx::BufferFormat to a (internal format, type) combination from the
// EGL_ANGLE_iosurface_client_buffer extension spec.
InternalFormatType BufferFormatToInternalFormatType(gfx::BufferFormat format,
                                                    bool emulate_rgb) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return {GL_RED, GL_UNSIGNED_BYTE};
    case gfx::BufferFormat::R_16:
      return {GL_RED_INTEGER, GL_UNSIGNED_SHORT};
    case gfx::BufferFormat::RG_88:
      return {GL_RG, GL_UNSIGNED_BYTE};
    case gfx::BufferFormat::BGRX_8888:
      if (emulate_rgb) {
        return {GL_BGRA_EXT, GL_UNSIGNED_BYTE};
      } else {
        return {GL_RGB, GL_UNSIGNED_BYTE};
      }
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::RGBA_8888:
      return {GL_BGRA_EXT, GL_UNSIGNED_BYTE};
    case gfx::BufferFormat::RGBA_F16:
      return {GL_RGBA, GL_HALF_FLOAT};
    case gfx::BufferFormat::BGRA_1010102:
      return {GL_RGB10_A2, GL_UNSIGNED_INT_2_10_10_10_REV};
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
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

GLImageIOSurfaceEGL::GLImageIOSurfaceEGL(const gfx::Size& size,
                                         unsigned internalformat,
                                         bool emulate_rgb)
    : GLImageIOSurface(size, internalformat),
      emulate_rgb_(emulate_rgb),
      display_(GLSurfaceEGL::GetHardwareDisplay()),
      pbuffer_(EGL_NO_SURFACE),
      dummy_config_(nullptr),
      texture_target_(EGL_TEXTURE_RECTANGLE_ANGLE),
      texture_bound_(false) {
  DCHECK(display_ != EGL_NO_DISPLAY);

  // When creating a pbuffer we need to supply an EGLConfig. On ANGLE and
  // Swiftshader on Mac, there's only ever one config. Query it from EGL.
  EGLint numConfigs = 0;
  EGLBoolean result =
      eglChooseConfig(display_, nullptr, &dummy_config_, 1, &numConfigs);
  DCHECK(result == EGL_TRUE);
  DCHECK(numConfigs = 1);
  DCHECK(dummy_config_ != nullptr);
  const char* extensions = eglQueryString(display_, EGL_EXTENSIONS);
  if (GLSurface::ExtensionsContain(extensions,
                                   "EGL_ANGLE_iosurface_client_buffer")) {
    result =
        eglGetConfigAttrib(display_, dummy_config_,
                           EGL_BIND_TO_TEXTURE_TARGET_ANGLE, &texture_target_);
    DCHECK(result == EGL_TRUE);
  }
  DCHECK(texture_target_ != EGL_NO_TEXTURE);
}

GLImageIOSurfaceEGL::~GLImageIOSurfaceEGL() {
  GLint target_gl = GLTargetFromEGLTarget(texture_target_);
  if (target_gl == GL_NONE) {
    return;
  }
  if (texture_bound_) {
    ReleaseTexImage(target_gl);
  }
  if (pbuffer_ != EGL_NO_SURFACE) {
    EGLBoolean result = eglDestroySurface(display_, pbuffer_);
    DCHECK(result == EGL_TRUE);
  }
}

void GLImageIOSurfaceEGL::ReleaseTexImage(unsigned target) {
  EGLint target_egl = EGLTargetFromGLTarget(target);
  if (target_egl == EGL_NO_TEXTURE) {
    return;
  }

  DCHECK(texture_target_ == target_egl);

  if (!texture_bound_) {
    return;
  }

  DCHECK(pbuffer_ != EGL_NO_SURFACE);

  EGLBoolean result = eglReleaseTexImage(display_, pbuffer_, EGL_BACK_BUFFER);
  DCHECK(result == EGL_TRUE);
  texture_bound_ = false;
}

bool GLImageIOSurfaceEGL::BindTexImageImpl(unsigned target,
                                           unsigned internalformat) {
  // TODO(cwallez@chromium.org): internalformat is used by Blink's
  // DrawingBuffer::SetupRGBEmulationForBlitFramebuffer to bind an RGBA
  // IOSurface as RGB. We should support this.

  if (texture_bound_) {
    LOG(ERROR) << "Cannot re-bind already bound IOSurface.";
    return false;
  }

  GLenum target_getter = TargetGetterFromGLTarget(target);
  EGLint target_egl = EGLTargetFromGLTarget(target);
  if (target_getter == GL_NONE || target_egl == EGL_NO_TEXTURE) {
    return false;
  }

  DCHECK(texture_target_ == target_egl);

  if (internalformat != 0) {
    LOG(ERROR) << "GLImageIOSurfaceEGL doesn't support binding with a custom "
                  "internal format yet.";
    return false;
  }

  // Create the pbuffer representing this IOSurface lazily because we don't know
  // in the constructor if we're going to be used to bind plane 0 to a texture,
  // or to transform YUV to RGB.
  if (pbuffer_ == EGL_NO_SURFACE) {
    InternalFormatType formatType =
        BufferFormatToInternalFormatType(format_, emulate_rgb_);

    // clang-format off
    const EGLint attribs[] = {
      EGL_WIDTH,                         size_.width(),
      EGL_HEIGHT,                        size_.height(),
      EGL_IOSURFACE_PLANE_ANGLE,         0,
      EGL_TEXTURE_TARGET,                texture_target_,
      EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, formatType.format,
      EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
      EGL_TEXTURE_TYPE_ANGLE,            formatType.type,
      EGL_NONE,                          EGL_NONE,
    };
    // clang-format on

    pbuffer_ = eglCreatePbufferFromClientBuffer(display_, EGL_IOSURFACE_ANGLE,
        io_surface_.get(), dummy_config_, attribs);
    if (pbuffer_ == EGL_NO_SURFACE) {
      LOG(ERROR) << "eglCreatePbufferFromClientBuffer failed, EGL error is "
                 << eglGetError();
      return false;
    }
  }

  DCHECK(!texture_bound_);
  EGLBoolean result = eglBindTexImage(display_, pbuffer_, EGL_BACK_BUFFER);

  if (result != EGL_TRUE) {
    LOG(ERROR) << "eglBindTexImage failed, EGL error is "
               << eglGetError();
    return false;
  }

  texture_bound_ = true;
  return true;
}

bool GLImageIOSurfaceEGL::CopyTexImage(unsigned target) {
  DCHECK(thread_checker_.CalledOnValidThread());

  GLint target_gl = GLTargetFromEGLTarget(texture_target_);
  if (target_gl == GL_NONE) {
    return false;
  }

  if (format_ != gfx::BufferFormat::YUV_420_BIPLANAR &&
      format_ != gfx::BufferFormat::P010) {
    LOG(ERROR) << "non-YUV buffer format passed to CopyTexImage";
    return false;
  }

  GLContext* gl_context = GLContext::GetCurrent();
  DCHECK(gl_context);

  YUVToRGBConverter* yuv_to_rgb_converter =
      gl_context->GetYUVToRGBConverter(color_space_for_yuv_to_rgb_);
  DCHECK(yuv_to_rgb_converter);

  // Note that state restoration is done explicitly instead of scoped binders to
  // avoid https://crbug.com/601729.
  GLint rgb_texture = 0;
  GLenum target_getter = TargetGetterFromGLTarget(target);
  if (target_getter == GL_NONE) {
    return false;
  }

  EGLSurface y_surface = EGL_NO_SURFACE;
  EGLSurface uv_surface = EGL_NO_SURFACE;

  EGLSurface* y_surface_ptr = &y_surface;
  EGLSurface* uv_surface_ptr = &uv_surface;
  glGetIntegerv(target_getter, &rgb_texture);
  base::ScopedClosureRunner destroy_resources_runner(
      base::BindOnce(base::RetainBlock(^{
        if (*y_surface_ptr != EGL_NO_SURFACE) {
          EGLBoolean result =
              eglReleaseTexImage(display_, *y_surface_ptr, EGL_BACK_BUFFER);
          DCHECK(result == EGL_TRUE);
          result = eglDestroySurface(display_, *y_surface_ptr);
          DCHECK(result == EGL_TRUE);
        }
        if (*uv_surface_ptr != EGL_NO_SURFACE) {
          EGLBoolean result =
              eglReleaseTexImage(display_, *uv_surface_ptr, EGL_BACK_BUFFER);
          DCHECK(result == EGL_TRUE);
          result = eglDestroySurface(display_, *uv_surface_ptr);
          DCHECK(result == EGL_TRUE);
        }
        glBindTexture(target, rgb_texture);
      })));

  glBindTexture(target_gl, yuv_to_rgb_converter->y_texture());
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Can't bind Y texture";
    return false;
  }

  // Disable mipmap filtering since iosurface doesn't have mipmap. Rectangle
  // textures have mipmap disabled by default but other types of texture don't.
  glTexParameteri(target_gl, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(target_gl, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target_gl, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  const EGLint texture_type =
      format_ == gfx::BufferFormat::P010 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_BYTE;

  // clang-format off
  const EGLint yAttribs[] = {
    EGL_WIDTH,                         size_.width(),
    EGL_HEIGHT,                        size_.height(),
    EGL_IOSURFACE_PLANE_ANGLE,         0,
    EGL_TEXTURE_TARGET,                texture_target_,
    EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_RED,
    EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
    EGL_TEXTURE_TYPE_ANGLE,            texture_type,
    EGL_NONE,                          EGL_NONE,
  };
  // clang-format on

  y_surface = eglCreatePbufferFromClientBuffer(display_, EGL_IOSURFACE_ANGLE,
                                               io_surface_.get(), dummy_config_,
                                               yAttribs);
  if (y_surface == EGL_NO_SURFACE) {
    LOG(ERROR) << "eglCreatePbufferFromClientBuffer failed, EGL error is "
               << eglGetError();
    return false;
  }

  EGLBoolean result = eglBindTexImage(display_, y_surface, EGL_BACK_BUFFER);
  if (result != EGL_TRUE) {
    LOG(ERROR) << "eglBindTexImage failed, EGL error is " << eglGetError();
    return false;
  }

  glBindTexture(target_gl, yuv_to_rgb_converter->uv_texture());
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Can't bind UV texture";
    return false;
  }

  glTexParameteri(target_gl, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(target_gl, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(target_gl, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  // clang-format off
  const EGLint uvAttribs[] = {
    EGL_WIDTH,                         size_.width() / 2,
    EGL_HEIGHT,                        size_.height() / 2,
    EGL_IOSURFACE_PLANE_ANGLE,         1,
    EGL_TEXTURE_TARGET,                texture_target_,
    EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, GL_RG,
    EGL_TEXTURE_FORMAT,                EGL_TEXTURE_RGBA,
    EGL_TEXTURE_TYPE_ANGLE,            texture_type,
    EGL_NONE,                          EGL_NONE,
  };
  // clang-format on

  uv_surface = eglCreatePbufferFromClientBuffer(display_, EGL_IOSURFACE_ANGLE,
                                                io_surface_.get(),
                                                dummy_config_, uvAttribs);
  if (uv_surface == EGL_NO_SURFACE) {
    LOG(ERROR) << "eglCreatePbufferFromClientBuffer failed, EGL error is "
               << eglGetError();
    return false;
  }

  result = eglBindTexImage(display_, uv_surface, EGL_BACK_BUFFER);
  if (result != EGL_TRUE) {
    LOG(ERROR) << "eglBindTexImage failed, EGL error is " << eglGetError();
    return false;
  }

  yuv_to_rgb_converter->CopyYUV420ToRGB(target, size_, rgb_texture,
                                        texture_type);
  if (glGetError() != GL_NO_ERROR) {
    LOG(ERROR) << "Failed converting from YUV to RGB";
    return false;
  }

  return true;
}

}  // namespace gl
