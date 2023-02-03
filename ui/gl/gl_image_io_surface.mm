// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_io_surface.h"

#include <map>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/mac/io_surface.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"

#if BUILDFLAG(IS_MAC)
#include "ui/gfx/mac/display_icc_profiles.h"
#endif

using gfx::BufferFormat;

namespace gl {

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
    case BufferFormat::YUVA_420_TRIPLANAR:
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
  color_space_ = color_space;
  return true;
}

gfx::Size GLImageIOSurface::GetSize() {
  return size_;
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

void GLImageIOSurface::SetColorSpace(const gfx::ColorSpace& color_space) {
  if (color_space_ == color_space)
    return;
  color_space_ = color_space;

#if BUILDFLAG(IS_MAC)
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
#endif

  // Only if that fails, fall back to IOSurfaceSetColorSpace, which will
  // generate a profile.
  IOSurfaceSetColorSpace(io_surface_, color_space);
}

}  // namespace gl
