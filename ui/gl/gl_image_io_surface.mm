// Copyright 2013 The Chromium Authors. All rights reserved.
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
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/yuv_to_rgb_converter.h"

#if defined(USE_EGL)
#include "ui/gl/gl_image_io_surface_egl.h"
#include "ui/gl/gl_implementation.h"
#endif  // defined(USE_EGL)

// Note that this must be included after gl_bindings.h to avoid conflicts.
#include <OpenGL/CGLIOSurface.h>
#include <Quartz/Quartz.h>
#include <stddef.h>

using gfx::BufferFormat;

namespace gl {
namespace {

bool ValidInternalFormat(unsigned internalformat) {
  switch (internalformat) {
    case GL_RED:
    case GL_R16_EXT:
    case GL_RG:
    case GL_BGRA_EXT:
    case GL_RGB:
    case GL_RGB10_A2_EXT:
    case GL_RGB_YCBCR_420V_CHROMIUM:
    case GL_RGB_YCBCR_422_CHROMIUM:
    case GL_RGBA:
      return true;
    default:
      return false;
  }
}

GLenum TextureFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return GL_RED;
    case gfx::BufferFormat::R_16:
      return GL_R16_EXT;
    case gfx::BufferFormat::RG_88:
      return GL_RG;
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:  // See https://crbug.com/595948.
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBA_F16:
      return GL_RGBA;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return GL_RGB_YCBCR_420V_CHROMIUM;
    case gfx::BufferFormat::BGRX_1010102:
      // Technically we should use GL_RGB but CGLTexImageIOSurface2D() (and
      // OpenGL ES 3.0, for the case) support only GL_RGBA (the hardware ignores
      // the alpha channel anyway), see https://crbug.com/797347.
      return GL_RGBA;
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::P010:
      NOTREACHED() << gfx::BufferFormatToString(format);
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLenum DataFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return GL_RED;
    case gfx::BufferFormat::R_16:
      return GL_R16_EXT;
    case gfx::BufferFormat::RG_88:
      return GL_RG;
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:  // See https://crbug.com/533677#c6.
    case gfx::BufferFormat::BGRX_1010102:
      return GL_BGRA;
    case gfx::BufferFormat::RGBA_F16:
      return GL_RGBA;
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::P010:
      NOTREACHED() << gfx::BufferFormatToString(format);
      return 0;
  }

  NOTREACHED();
  return 0;
}

GLenum DataType(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RG_88:
      return GL_UNSIGNED_BYTE;
    case gfx::BufferFormat::R_16:
      return GL_UNSIGNED_SHORT;
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
      return GL_UNSIGNED_INT_8_8_8_8_REV;
    case gfx::BufferFormat::BGRX_1010102:
      return GL_UNSIGNED_INT_2_10_10_10_REV;
    case gfx::BufferFormat::RGBA_F16:
      return GL_HALF_APPLE;
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::P010:
      NOTREACHED() << gfx::BufferFormatToString(format);
      return 0;
  }

  NOTREACHED();
  return 0;
}

// When an IOSurface is bound to a texture with internalformat "GL_RGB", many
// OpenGL operations are broken. Therefore, don't allow an IOSurface to be bound
// with GL_RGB unless overridden via BindTexImageWithInternalformat.
// https://crbug.com/595948, https://crbug.com/699566.
GLenum ConvertRequestedInternalFormat(GLenum internalformat) {
  if (internalformat == GL_RGB)
    return GL_RGBA;
  return internalformat;
}

}  // namespace

// static
GLImageIOSurface* GLImageIOSurface::Create(const gfx::Size& size,
                                           unsigned internalformat) {
#if defined(USE_EGL)
  switch (GetGLImplementation()) {
    case kGLImplementationEGLGLES2:
    case kGLImplementationEGLANGLE:
    case kGLImplementationSwiftShaderGL:
      return new GLImageIOSurfaceEGL(
          size, internalformat,
          GetGLImplementation() == kGLImplementationSwiftShaderGL);
    default:
      break;
  }
#endif  // defined(USE_EGL)

  return new GLImageIOSurface(size, internalformat);
}

GLImageIOSurface::GLImageIOSurface(const gfx::Size& size,
                                   unsigned internalformat)
    : size_(size),
      internalformat_(ConvertRequestedInternalFormat(internalformat)),
      client_internalformat_(internalformat),
      format_(gfx::BufferFormat::RGBA_8888) {}

GLImageIOSurface::~GLImageIOSurface() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

bool GLImageIOSurface::Initialize(IOSurfaceRef io_surface,
                                  gfx::GenericSharedMemoryId io_surface_id,
                                  gfx::BufferFormat format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!io_surface_);
  if (!io_surface) {
    LOG(ERROR) << "Invalid IOSurface";
    return false;
  }

  if (!ValidInternalFormat(internalformat_)) {
    LOG(ERROR) << "Invalid internalformat: "
               << GLEnums::GetStringEnum(internalformat_);
    return false;
  }

  if (!ValidFormat(format)) {
    LOG(ERROR) << "Invalid format: " << gfx::BufferFormatToString(format);
    return false;
  }

  format_ = format;
  io_surface_.reset(io_surface, base::scoped_policy::RETAIN);
  io_surface_id_ = io_surface_id;
  return true;
}

bool GLImageIOSurface::InitializeWithCVPixelBuffer(
    CVPixelBufferRef cv_pixel_buffer,
    gfx::GenericSharedMemoryId io_surface_id,
    gfx::BufferFormat format) {
  IOSurfaceRef io_surface = CVPixelBufferGetIOSurface(cv_pixel_buffer);
  if (!io_surface) {
    LOG(ERROR) << "Can't init GLImage from CVPixelBuffer with no IOSurface";
    return false;
  }

  if (!Initialize(io_surface, io_surface_id, format))
    return false;

  cv_pixel_buffer_.reset(cv_pixel_buffer, base::scoped_policy::RETAIN);
  return true;
}

gfx::Size GLImageIOSurface::GetSize() {
  return size_;
}

unsigned GLImageIOSurface::GetInternalFormat() {
  return internalformat_;
}

unsigned GLImageIOSurface::GetDataType() {
  return gl::BufferFormatToGLDataType(format_);
}

GLImageIOSurface::BindOrCopy GLImageIOSurface::ShouldBindOrCopy() {
  // YUV_420_BIPLANAR is not supported by BindTexImage.
  // CopyTexImage is supported by this format as that performs conversion to RGB
  // as part of the copy operation.
  return format_ == gfx::BufferFormat::YUV_420_BIPLANAR ? COPY : BIND;
}

bool GLImageIOSurface::BindTexImage(unsigned target) {
  return BindTexImageWithInternalformat(target, 0);
}

bool GLImageIOSurface::BindTexImageWithInternalformat(unsigned target,
                                                      unsigned internalformat) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(BIND, ShouldBindOrCopy());
  TRACE_EVENT0("gpu", "GLImageIOSurface::BindTexImage");
  base::TimeTicks start_time = base::TimeTicks::Now();

  if (target != GL_TEXTURE_RECTANGLE_ARB) {
    // This might be supported in the future. For now, perform strict
    // validation so we know what's going on.
    LOG(ERROR) << "IOSurface requires TEXTURE_RECTANGLE_ARB target";
    return false;
  }

  DCHECK(io_surface_);

  if (!BindTexImageImpl(internalformat)) {
    return false;
  }

  UMA_HISTOGRAM_TIMES("GPU.IOSurface.TexImageTime",
                      base::TimeTicks::Now() - start_time);
  return true;
}

bool GLImageIOSurface::BindTexImageImpl(unsigned internalformat) {
  CGLContextObj cgl_context =
      static_cast<CGLContextObj>(GLContext::GetCurrent()->GetHandle());

  GLenum texture_format =
      internalformat ? internalformat : TextureFormat(format_);
  CGLError cgl_error = CGLTexImageIOSurface2D(
      cgl_context, GL_TEXTURE_RECTANGLE_ARB, texture_format, size_.width(),
      size_.height(), DataFormat(format_), DataType(format_), io_surface_.get(),
      0);
  if (cgl_error != kCGLNoError) {
    LOG(ERROR) << "Error in CGLTexImageIOSurface2D: "
               << CGLErrorString(cgl_error);
    return false;
  }

  return true;
}

bool GLImageIOSurface::CopyTexImage(unsigned target) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(COPY, ShouldBindOrCopy());

  GLContext* gl_context = GLContext::GetCurrent();
  DCHECK(gl_context);

  YUVToRGBConverter* yuv_to_rgb_converter =
      gl_context->GetYUVToRGBConverter(color_space_for_yuv_to_rgb_);
  DCHECK(yuv_to_rgb_converter);

  // Note that state restoration is done explicitly instead of scoped binders to
  // avoid https://crbug.com/601729.
  GLint rgb_texture = 0;
  GLenum target_getter = 0;
  switch (target) {
    case GL_TEXTURE_2D:
      target_getter = GL_TEXTURE_BINDING_2D;
      break;
    case GL_TEXTURE_CUBE_MAP:
      target_getter = GL_TEXTURE_BINDING_CUBE_MAP;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      target_getter = GL_TEXTURE_BINDING_EXTERNAL_OES;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      target_getter = GL_TEXTURE_BINDING_RECTANGLE_ARB;
      break;
    default:
      NOTIMPLEMENTED() << " Target not supported.";
      return false;
  }
  glGetIntegerv(target_getter, &rgb_texture);
  base::ScopedClosureRunner destroy_resources_runner(
      base::BindOnce(base::RetainBlock(^{
        glBindTexture(target, rgb_texture);
      })));

  CGLContextObj cgl_context = CGLGetCurrentContext();
  {
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, yuv_to_rgb_converter->y_texture());
    CGLError cgl_error = CGLTexImageIOSurface2D(
        cgl_context, GL_TEXTURE_RECTANGLE_ARB, GL_RED, size_.width(),
        size_.height(), GL_RED, GL_UNSIGNED_BYTE, io_surface_, 0);
    if (cgl_error != kCGLNoError) {
      LOG(ERROR) << "Error in CGLTexImageIOSurface2D for the Y plane. "
                 << cgl_error;
      return false;
    }
  }
  {
    glBindTexture(GL_TEXTURE_RECTANGLE_ARB, yuv_to_rgb_converter->uv_texture());
    CGLError cgl_error = CGLTexImageIOSurface2D(
        cgl_context, GL_TEXTURE_RECTANGLE_ARB, GL_RG, size_.width() / 2,
        size_.height() / 2, GL_RG, GL_UNSIGNED_BYTE, io_surface_, 1);
    if (cgl_error != kCGLNoError) {
      LOG(ERROR) << "Error in CGLTexImageIOSurface2D for the UV plane. "
                 << cgl_error;
      return false;
    }
  }

  yuv_to_rgb_converter->CopyYUV420ToRGB(target, size_, rgb_texture);
  return true;
}

bool GLImageIOSurface::CopyTexSubImage(unsigned target,
                                       const gfx::Point& offset,
                                       const gfx::Rect& rect) {
  return false;
}

bool GLImageIOSurface::ScheduleOverlayPlane(
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

void GLImageIOSurface::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                    uint64_t process_tracing_id,
                                    const std::string& dump_name) {
  // IOSurfaceGetAllocSize will return 0 if io_surface_ is invalid. In this case
  // we log 0 for consistency with other GLImage memory dump functions.
  size_t size_bytes = IOSurfaceGetAllocSize(io_surface_);

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
  }
}

bool GLImageIOSurface::EmulatingRGB() const {
  return client_internalformat_ == GL_RGB;
}

bool GLImageIOSurface::CanCheckIOSurfaceIsInUse() const {
  return !cv_pixel_buffer_;
}

void GLImageIOSurface::SetColorSpaceForYUVToRGBConversion(
    const gfx::ColorSpace& color_space) {
  DCHECK(color_space.IsValid());
  DCHECK_NE(color_space, color_space.GetAsFullRangeRGB());
  color_space_for_yuv_to_rgb_ = color_space;
}

base::ScopedCFTypeRef<IOSurfaceRef> GLImageIOSurface::io_surface() {
  return io_surface_;
}

base::ScopedCFTypeRef<CVPixelBufferRef> GLImageIOSurface::cv_pixel_buffer() {
  return cv_pixel_buffer_;
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
unsigned GLImageIOSurface::GetInternalFormatForTesting(
    gfx::BufferFormat format) {
  DCHECK(ValidFormat(format));
  return TextureFormat(format);
}

// static
GLImageIOSurface* GLImageIOSurface::FromGLImage(GLImage* image) {
  if (!image || image->GetType() != Type::IOSURFACE)
    return nullptr;
  return static_cast<GLImageIOSurface*>(image);
}

// static
bool GLImageIOSurface::ValidFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return true;
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::P010:
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace gl
