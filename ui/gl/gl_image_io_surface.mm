// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_io_surface.h"

#include <map>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/display_icc_profiles.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"

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
    case BufferFormat::RGBA_8888:
      return {GL_BGRA_EXT, GL_UNSIGNED_BYTE};
    case BufferFormat::RGBA_F16:
      return {GL_RGBA, GL_HALF_FLOAT};
    case BufferFormat::BGRA_1010102:
      return {GL_RGB10_A2, GL_UNSIGNED_INT_2_10_10_10_REV};
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_1010102:
    case BufferFormat::YVU_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::P010:
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

////////////////////////////////////////////////////////////////////////////////
// EGLAccess

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

////////////////////////////////////////////////////////////////////////////////
// GLImageIOSurface

// static
GLImageIOSurface* GLImageIOSurface::Create(const gfx::Size& size) {
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
      return new GLImageIOSurface(size);
    default:
      break;
  }
  NOTREACHED();
  return nullptr;
}

GLImageIOSurface::GLImageIOSurface(const gfx::Size& size) : size_(size) {}

GLImageIOSurface::~GLImageIOSurface() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (texture_bound_) {
    GLint target_gl =
        GLTargetFromEGLTarget(GetEGLAccessForCurrentContext().texture_target());
    DCHECK_NE(target_gl, GL_NONE);
    ReleaseTexImage(target_gl);
  }
}

bool GLImageIOSurface::Initialize(IOSurfaceRef io_surface,
                                  uint32_t io_surface_plane,
                                  gfx::GenericSharedMemoryId io_surface_id,
                                  BufferFormat format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!io_surface_);
  if (!io_surface) {
    LOG(ERROR) << "Invalid IOSurface";
    return false;
  }

  switch (format) {
    case BufferFormat::R_8:
    case BufferFormat::RG_88:
    case BufferFormat::R_16:
    case BufferFormat::RG_1616:
    case BufferFormat::BGRA_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::RGBA_8888:
    case BufferFormat::RGBX_8888:
    case BufferFormat::RGBA_F16:
    case BufferFormat::BGRA_1010102:
      break;
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::P010:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBA_1010102:
    case BufferFormat::YVU_420:
      LOG(ERROR) << "Invalid format: " << BufferFormatToString(format);
      return false;
  }

  format_ = format;
  io_surface_.reset(io_surface, base::scoped_policy::RETAIN);
  io_surface_id_ = io_surface_id;
  io_surface_plane_ = io_surface_plane;
  return true;
}

bool GLImageIOSurface::InitializeWithCVPixelBuffer(
    CVPixelBufferRef cv_pixel_buffer,
    uint32_t io_surface_plane,
    gfx::GenericSharedMemoryId io_surface_id,
    BufferFormat format,
    const gfx::ColorSpace& color_space) {
  IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(cv_pixel_buffer);
  if (!io_surface) {
    LOG(ERROR) << "Can't init GLImage from CVPixelBuffer with no IOSurface";
    return false;
  }

  if (!Initialize(io_surface, io_surface_plane, io_surface_id, format))
    return false;

  cv_pixel_buffer_.reset(cv_pixel_buffer, base::scoped_policy::RETAIN);
  disable_in_use_by_window_server_ = true;
  GLImage::SetColorSpace(color_space);
  return true;
}

gfx::Size GLImageIOSurface::GetSize() {
  return size_;
}

unsigned GLImageIOSurface::GetInternalFormat() {
  return BufferFormatToGLInternalFormat(format_);
}

unsigned GLImageIOSurface::GetDataType() {
  return BufferFormatToGLDataType(format_);
}

GLImage::BindOrCopy GLImageIOSurface::ShouldBindOrCopy() {
  return BIND;
}

EGLAccess& GLImageIOSurface::GetEGLAccessForCurrentContext() {
  GLDisplayEGL* display = GLDisplayEGL::GetDisplayForCurrentContext();
  DCHECK_NE(display, nullptr);
  if (!egl_access_map_.contains(display)) {
    egl_access_map_.emplace(display, EGLAccess(display));
  }
  return egl_access_map_.at(display);
}

bool GLImageIOSurface::BindTexImage(unsigned target) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(BIND, ShouldBindOrCopy());
  TRACE_EVENT0("gpu", "GLImageIOSurface::BindTexImage");
  base::TimeTicks start_time = base::TimeTicks::Now();
  CHECK(!texture_bound_) << "Cannot re-bind already bound IOSurface.";

  GLenum target_getter = TargetGetterFromGLTarget(target);
  EGLint target_egl = EGLTargetFromGLTarget(target);
  if (target_getter == GL_NONE || target_egl == EGL_NO_TEXTURE) {
    return false;
  }

  EGLAccess& egl_access = GetEGLAccessForCurrentContext();

  DCHECK_EQ(egl_access.texture_target(), target_egl);

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
    LOG(ERROR) << "eglBindTexImage failed, EGL error is " << eglGetError();
    eglDestroySurface(egl_access.display()->GetDisplay(), egl_access.pbuffer());
    egl_access.set_pbuffer(EGL_NO_SURFACE);
    return false;
  }

  texture_bound_ = true;
  UMA_HISTOGRAM_TIMES("GPU.IOSurface.TexImageTime",
                      base::TimeTicks::Now() - start_time);
  return true;
}

void GLImageIOSurface::ReleaseTexImage(unsigned target) {
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

void GLImageIOSurface::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                    uint64_t process_tracing_id,
                                    const std::string& dump_name) {
  size_t size_bytes = 0;
  if (io_surface_) {
    if (io_surface_plane_ == kInvalidIOSurfacePlane) {
      size_bytes = IOSurfaceGetAllocSize(io_surface_);
    } else {
      size_bytes =
          IOSurfaceGetBytesPerRowOfPlane(io_surface_, io_surface_plane_) *
          IOSurfaceGetHeightOfPlane(io_surface_, io_surface_plane_);
    }
  }

  base::trace_event::MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                  static_cast<uint64_t>(size_bytes));

  // The process tracing id is to identify the GpuMemoryBuffer client that
  // created the allocation. For CVPixelBufferRefs, there is no corresponding
  // GpuMemoryBuffer, so use an invalid process id.
  if (cv_pixel_buffer_) {
    process_tracing_id =
        base::trace_event::MemoryDumpManager::kInvalidTracingProcessId;
  }

  // Create an edge using the GMB GenericSharedMemoryId if the image is not
  // anonymous. Otherwise, add another nested node to account for the anonymous
  // IOSurface.
  if (io_surface_id_.is_valid()) {
    auto guid = GetGenericSharedGpuMemoryGUIDForTracing(process_tracing_id,
                                                        io_surface_id_);
    pmd->CreateSharedGlobalAllocatorDump(guid);
    pmd->AddOwnershipEdge(dump->guid(), guid);
  } else {
    std::string anonymous_dump_name = dump_name + "/anonymous-iosurface";
    base::trace_event::MemoryAllocatorDump* anonymous_dump =
        pmd->CreateAllocatorDump(anonymous_dump_name);
    anonymous_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
        static_cast<uint64_t>(size_bytes));
    anonymous_dump->AddScalar("width", "pixels", size_.width());
    anonymous_dump->AddScalar("height", "pixels", size_.height());
  }
}

bool GLImageIOSurface::IsInUseByWindowServer() const {
  // IOSurfaceIsInUse() will always return true if the IOSurface is wrapped in
  // a CVPixelBuffer. Ignore the signal for such IOSurfaces (which are the ones
  // output by hardware video decode).
  if (disable_in_use_by_window_server_)
    return false;
  return IOSurfaceIsInUse(io_surface_.get());
}

void GLImageIOSurface::DisableInUseByWindowServer() {
  disable_in_use_by_window_server_ = true;
}

GLImage::Type GLImageIOSurface::GetType() const {
  return Type::IOSURFACE;
}

void GLImageIOSurface::SetColorSpace(const gfx::ColorSpace& color_space) {
  if (color_space_ == color_space)
    return;
  GLImage::SetColorSpace(color_space);

  // Prefer to use data from DisplayICCProfiles, which will give a byte-for-byte
  // match for color spaces of the system displays. Note that DisplayICCProfiles
  // is not used in IOSurfaceSetColorSpace because that call may be made in the
  // renderer process (e.g, for video frames).
  base::ScopedCFTypeRef<CFDataRef> cf_data =
      gfx::DisplayICCProfiles::GetInstance()->GetDataForColorSpace(color_space);
  if (cf_data) {
    IOSurfaceSetValue(io_surface_, CFSTR("IOSurfaceColorSpace"), cf_data);
    return;
  }

  // Only if that fails, fall back to IOSurfaceSetColorSpace, which will
  // generate a profile.
  IOSurfaceSetColorSpace(io_surface_, color_space);
}

// static
GLImageIOSurface* GLImageIOSurface::FromGLImage(GLImage* image) {
  if (!image || image->GetType() != Type::IOSURFACE)
    return nullptr;
  return static_cast<GLImageIOSurface*>(image);
}

}  // namespace gl
