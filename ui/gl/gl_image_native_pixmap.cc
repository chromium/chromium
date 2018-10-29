// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_native_pixmap.h"

#include <vector>

#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_surface_egl.h"

#define FOURCC(a, b, c, d)                                        \
  ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
   (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

#define DRM_FORMAT_R8 FOURCC('R', '8', ' ', ' ')
#define DRM_FORMAT_R16 FOURCC('R', '1', '6', ' ')
#define DRM_FORMAT_GR88 FOURCC('G', 'R', '8', '8')
#define DRM_FORMAT_RGB565 FOURCC('R', 'G', '1', '6')
#define DRM_FORMAT_ARGB8888 FOURCC('A', 'R', '2', '4')
#define DRM_FORMAT_ABGR8888 FOURCC('A', 'B', '2', '4')
#define DRM_FORMAT_XRGB8888 FOURCC('X', 'R', '2', '4')
#define DRM_FORMAT_XBGR8888 FOURCC('X', 'B', '2', '4')
#define DRM_FORMAT_ABGR2101010 FOURCC('A', 'B', '3', '0')
#define DRM_FORMAT_YVU420 FOURCC('Y', 'V', '1', '2')
#define DRM_FORMAT_NV12 FOURCC('N', 'V', '1', '2')

namespace gl {
namespace {

bool ValidInternalFormat(unsigned internalformat, gfx::BufferFormat format) {
  switch (internalformat) {
    case GL_RGB:
      return format == gfx::BufferFormat::BGR_565 ||
             format == gfx::BufferFormat::RGBX_8888 ||
             format == gfx::BufferFormat::BGRX_8888;
    case GL_RGB10_A2_EXT:
      return format == gfx::BufferFormat::RGBX_1010102;
    case GL_RGB_YCRCB_420_CHROMIUM:
      return format == gfx::BufferFormat::YVU_420;
    case GL_RGB_YCBCR_420V_CHROMIUM:
      return format == gfx::BufferFormat::YUV_420_BIPLANAR;
    case GL_RGBA:
      return format == gfx::BufferFormat::RGBA_8888 ||
             format == gfx::BufferFormat::RGBX_1010102;
    case GL_BGRA_EXT:
      return format == gfx::BufferFormat::BGRA_8888 ||
             format == gfx::BufferFormat::BGRX_1010102;
    case GL_RED_EXT:
      return format == gfx::BufferFormat::R_8;
    case GL_R16_EXT:
      return format == gfx::BufferFormat::R_16;
    case GL_RG_EXT:
      return format == gfx::BufferFormat::RG_88;
    default:
      return false;
  }
}

EGLint FourCC(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return DRM_FORMAT_R8;
    case gfx::BufferFormat::R_16:
      return DRM_FORMAT_R16;
    case gfx::BufferFormat::RG_88:
      return DRM_FORMAT_GR88;
    case gfx::BufferFormat::BGR_565:
      return DRM_FORMAT_RGB565;
    case gfx::BufferFormat::RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case gfx::BufferFormat::RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case gfx::BufferFormat::BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case gfx::BufferFormat::BGRX_8888:
      return DRM_FORMAT_XRGB8888;
    case gfx::BufferFormat::RGBX_1010102:
      // We should use here DRM_FORMAT_XBGR2101010 format, but EGL on Intel
      // doesn't support it for scanout, see https://crbug.com/776093#c14.
      return DRM_FORMAT_ABGR2101010;
    case gfx::BufferFormat::YVU_420:
      return DRM_FORMAT_YVU420;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return DRM_FORMAT_NV12;
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED();
      return 0;
  }

  NOTREACHED();
  return 0;
}

gfx::BufferFormat GetBufferFormatFromFourCCFormat(int format) {
  switch (format) {
    case DRM_FORMAT_R8:
      return gfx::BufferFormat::R_8;
    case DRM_FORMAT_GR88:
      return gfx::BufferFormat::RG_88;
    case DRM_FORMAT_ABGR8888:
      return gfx::BufferFormat::RGBA_8888;
    case DRM_FORMAT_XBGR8888:
      return gfx::BufferFormat::RGBX_8888;
    case DRM_FORMAT_ARGB8888:
      return gfx::BufferFormat::BGRA_8888;
    case DRM_FORMAT_XRGB8888:
      return gfx::BufferFormat::BGRX_8888;
    case DRM_FORMAT_ABGR2101010:
      // We should support DRM_FORMAT_XBGR2101010 format instead, but EGL on
      // Intel doesn't support it for scanout, see https://crbug.com/776093#c14.
      return gfx::BufferFormat::RGBX_1010102;
    case DRM_FORMAT_RGB565:
      return gfx::BufferFormat::BGR_565;
    case DRM_FORMAT_NV12:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    case DRM_FORMAT_YVU420:
      return gfx::BufferFormat::YVU_420;
    default:
      NOTREACHED();
      return gfx::BufferFormat::BGRA_8888;
  }
}

}  // namespace

GLImageNativePixmap::GLImageNativePixmap(const gfx::Size& size,
                                         unsigned internalformat)
    : GLImageEGL(size),
      internalformat_(internalformat),
      has_image_flush_external_(
          gl::GLSurfaceEGL::HasEGLExtension("EGL_EXT_image_flush_external")),
      has_image_dma_buf_export_(
          gl::GLSurfaceEGL::HasEGLExtension("EGL_MESA_image_dma_buf_export")) {}

GLImageNativePixmap::~GLImageNativePixmap() {}

bool GLImageNativePixmap::Initialize(gfx::NativePixmap* pixmap,
                                     gfx::BufferFormat format) {
  DCHECK(!pixmap_);
  if (pixmap->AreDmaBufFdsValid()) {
    if (!ValidFormat(format)) {
      LOG(ERROR) << "Invalid format: " << gfx::BufferFormatToString(format);
      return false;
    }

    if (!ValidInternalFormat(internalformat_, format)) {
      LOG(ERROR) << "Invalid internalformat: "
                 << GLEnums::GetStringEnum(internalformat_)
                 << " for format: " << gfx::BufferFormatToString(format);
      return false;
    }

    // Note: If eglCreateImageKHR is successful for a EGL_LINUX_DMA_BUF_EXT
    // target, the EGL will take a reference to the dma_buf.
    std::vector<EGLint> attrs;
    attrs.push_back(EGL_WIDTH);
    attrs.push_back(size_.width());
    attrs.push_back(EGL_HEIGHT);
    attrs.push_back(size_.height());
    attrs.push_back(EGL_LINUX_DRM_FOURCC_EXT);
    attrs.push_back(FourCC(format));

    const EGLint kLinuxDrmModifiers[] = {EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
                                         EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
                                         EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT};
    bool has_dma_buf_import_modifier = gl::GLSurfaceEGL::HasEGLExtension(
        "EGL_EXT_image_dma_buf_import_modifiers");

    for (size_t attrs_plane = 0;
         attrs_plane <
         gfx::NumberOfPlanesForBufferFormat(pixmap->GetBufferFormat());
         ++attrs_plane) {
      attrs.push_back(EGL_DMA_BUF_PLANE0_FD_EXT + attrs_plane * 3);

      size_t pixmap_plane = attrs_plane;

      attrs.push_back(pixmap->GetDmaBufFd(
          pixmap_plane < pixmap->GetDmaBufFdCount() ? pixmap_plane : 0));
      attrs.push_back(EGL_DMA_BUF_PLANE0_OFFSET_EXT + attrs_plane * 3);
      attrs.push_back(pixmap->GetDmaBufOffset(pixmap_plane));
      attrs.push_back(EGL_DMA_BUF_PLANE0_PITCH_EXT + attrs_plane * 3);
      attrs.push_back(pixmap->GetDmaBufPitch(pixmap_plane));
      if (has_dma_buf_import_modifier &&
          pixmap->GetDmaBufModifier(0) != gfx::NativePixmapPlane::kNoModifier) {
        uint64_t modifier = pixmap->GetDmaBufModifier(pixmap_plane);
        DCHECK(attrs_plane < arraysize(kLinuxDrmModifiers));
        attrs.push_back(kLinuxDrmModifiers[attrs_plane]);
        attrs.push_back(modifier & 0xffffffff);
        attrs.push_back(kLinuxDrmModifiers[attrs_plane] + 1);
        attrs.push_back(static_cast<uint32_t>(modifier >> 32));
      }
    }
    attrs.push_back(EGL_NONE);

    if (!GLImageEGL::Initialize(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                                static_cast<EGLClientBuffer>(nullptr),
                                &attrs[0])) {
      return false;
    }
  }

  pixmap_ = pixmap;
  return true;
}

bool GLImageNativePixmap::InitializeFromTexture(uint32_t texture_id) {
  GLContext* current_context = GLContext::GetCurrent();
  if (!current_context || !current_context->IsCurrent(nullptr)) {
    LOG(ERROR) << "No gl context bound to the current thread";
    return false;
  }

  EGLContext context_handle =
      reinterpret_cast<EGLContext>(current_context->GetHandle());
  DCHECK_NE(context_handle, EGL_NO_CONTEXT);

  return GLImageEGL::Initialize(context_handle, EGL_GL_TEXTURE_2D_KHR,
                                reinterpret_cast<EGLClientBuffer>(texture_id),
                                nullptr);
}

gfx::NativePixmapHandle GLImageNativePixmap::ExportHandle() {
  DCHECK(!pixmap_);
  DCHECK(thread_checker_.CalledOnValidThread());

  // Must use GLImageEGL::Initialize.
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    LOG(ERROR) << "GLImageEGL is not initialized";
    return gfx::NativePixmapHandle();
  }

  if (!has_image_dma_buf_export_) {
    LOG(ERROR) << "Error no extension EGL_MESA_image_dma_buf_export";
    return gfx::NativePixmapHandle();
  }

  int fourcc = 0;
  int num_planes = 0;
  EGLuint64KHR modifiers = 0;

  if (!eglExportDMABUFImageQueryMESA(GLSurfaceEGL::GetHardwareDisplay(),
                                     egl_image_, &fourcc, &num_planes,
                                     &modifiers)) {
    LOG(ERROR) << "Error querying EGLImage: " << ui::GetLastEGLErrorString();
    return gfx::NativePixmapHandle();
  }

  if (num_planes <= 0 || num_planes > 4) {
    LOG(ERROR) << "Invalid number of planes: " << num_planes;
    return gfx::NativePixmapHandle();
  }

  gfx::BufferFormat format = GetBufferFormatFromFourCCFormat(fourcc);
  if (num_planes > 0 && static_cast<size_t>(num_planes) !=
                            gfx::NumberOfPlanesForBufferFormat(format)) {
    LOG(ERROR) << "Invalid number of planes: " << num_planes
               << " for format: " << gfx::BufferFormatToString(format);
    return gfx::NativePixmapHandle();
  }

  if (!ValidInternalFormat(internalformat_, format)) {
    // A driver has returned a format different than what has been requested.
    // This can happen if RGBX is implemented using RGBA. Otherwise there is
    // a real mistake from the user and we have to fail.
    if (internalformat_ == GL_RGB && format != gfx::BufferFormat::RGBA_8888) {
      LOG(ERROR) << "Invalid internalformat: "
                 << GLEnums::GetStringEnum(internalformat_)
                 << " for format: " << gfx::BufferFormatToString(format);
      return gfx::NativePixmapHandle();
    }
  }

#if defined(OS_FUCHSIA)
  // TODO(crbug.com/852011): Implement image handle export on Fuchsia.
  NOTIMPLEMENTED();
  return gfx::NativePixmapHandle();
#else   // defined(OS_FUCHSIA)
  std::vector<int> fds(num_planes);
  std::vector<EGLint> strides(num_planes);
  std::vector<EGLint> offsets(num_planes);

  // It is specified for eglExportDMABUFImageMESA that the app is responsible
  // for closing any fds retrieved.
  if (!eglExportDMABUFImageMESA(GLSurfaceEGL::GetHardwareDisplay(), egl_image_,
                                &fds[0], &strides[0], &offsets[0])) {
    LOG(ERROR) << "Error exporting EGLImage: " << ui::GetLastEGLErrorString();
    return gfx::NativePixmapHandle();
  }

  gfx::NativePixmapHandle handle;

  for (int i = 0; i < num_planes; ++i) {
    // Sanity check. In principle all the fds are meant to be valid when
    // eglExportDMABUFImageMESA succeeds.
    base::ScopedFD scoped_fd(fds[i]);
    if (!scoped_fd.is_valid()) {
      LOG(ERROR) << "Invalid dmabuf";
      return gfx::NativePixmapHandle();
    }

    // scoped_fd.release() transfers ownership to the caller so it will not
    // call close when going out of scope. base::FileDescriptor never closes
    // the fd when going out of scope. The auto_close flag is just a hint for
    // the user. When true it means the user has ownership of it so they are
    // responsible for closing the fd.
    handle.fds.emplace_back(
        base::FileDescriptor(scoped_fd.release(), true /* auto_close */));
    handle.planes.emplace_back(strides[i], offsets[i], 0 /* size opaque */,
                               modifiers);
  }

  return handle;
#endif  // !defined(OS_FUCHSIA)
}

unsigned GLImageNativePixmap::GetInternalFormat() {
  return internalformat_;
}

bool GLImageNativePixmap::CopyTexImage(unsigned target) {
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    // Pass-through image type fails to bind and copy; make sure we
    // don't draw with uninitialized texture.
    std::vector<unsigned char> data(size_.width() * size_.height() * 4);
    glTexImage2D(target, 0, GL_RGBA, size_.width(), size_.height(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, data.data());
    return true;
  }
  return false;
}

bool GLImageNativePixmap::CopyTexSubImage(unsigned target,
                                          const gfx::Point& offset,
                                          const gfx::Rect& rect) {
  return false;
}

bool GLImageNativePixmap::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  DCHECK(pixmap_);
  return pixmap_->ScheduleOverlayPlane(widget, z_order, transform, bounds_rect,
                                       crop_rect, enable_blend,
                                       std::move(gpu_fence));
}

void GLImageNativePixmap::Flush() {
  if (!has_image_flush_external_)
    return;

  EGLDisplay display = gl::GLSurfaceEGL::GetHardwareDisplay();
  const EGLAttrib attribs[] = {
      EGL_NONE,
  };
  if (!eglImageFlushExternalEXT(display, egl_image_, attribs)) {
    // TODO(reveman): Investigate why we're hitting the following log
    // statement on ARM devices. b/63364517
    DLOG(WARNING) << "Failed to flush rendering";
    return;
  }
}

void GLImageNativePixmap::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {
  // TODO(ericrk): Implement GLImage OnMemoryDump. crbug.com/514914
}

// static
unsigned GLImageNativePixmap::GetInternalFormatForTesting(
    gfx::BufferFormat format) {
  DCHECK(ValidFormat(format));
  switch (format) {
    case gfx::BufferFormat::R_8:
      return GL_RED_EXT;
    case gfx::BufferFormat::R_16:
      return GL_R16_EXT;
    case gfx::BufferFormat::RG_88:
      return GL_RG_EXT;
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRX_8888:
      return GL_RGB;
    case gfx::BufferFormat::RGBA_8888:
      return GL_RGBA;
    case gfx::BufferFormat::RGBX_1010102:
      return GL_RGB10_A2_EXT;
    case gfx::BufferFormat::BGRA_8888:
      return GL_BGRA_EXT;
    case gfx::BufferFormat::YVU_420:
      return GL_RGB_YCRCB_420_CHROMIUM;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return GL_RGB_YCBCR_420V_CHROMIUM;
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::UYVY_422:
      NOTREACHED();
      return GL_NONE;
  }

  NOTREACHED();
  return GL_NONE;
}

// static
bool GLImageNativePixmap::ValidFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::R_16:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::BGR_565:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return true;
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::BGRX_1010102:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::UYVY_422:
      return false;
  }

  NOTREACHED();
  return false;
}

}  // namespace gl
