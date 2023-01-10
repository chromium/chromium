// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_native_pixmap.h"

#include <vector>

#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_enums.h"
#include "ui/gl/gl_surface_egl.h"

#define FOURCC(a, b, c, d)                                        \
  ((static_cast<uint32_t>(a)) | (static_cast<uint32_t>(b) << 8) | \
   (static_cast<uint32_t>(c) << 16) | (static_cast<uint32_t>(d) << 24))

#define DRM_FORMAT_R8 FOURCC('R', '8', ' ', ' ')
#define DRM_FORMAT_R16 FOURCC('R', '1', '6', ' ')
#define DRM_FORMAT_GR88 FOURCC('G', 'R', '8', '8')
#define DRM_FORMAT_GR1616 FOURCC('G', 'R', '3', '2')
#define DRM_FORMAT_RGB565 FOURCC('R', 'G', '1', '6')
#define DRM_FORMAT_ARGB8888 FOURCC('A', 'R', '2', '4')
#define DRM_FORMAT_ABGR8888 FOURCC('A', 'B', '2', '4')
#define DRM_FORMAT_XRGB8888 FOURCC('X', 'R', '2', '4')
#define DRM_FORMAT_XBGR8888 FOURCC('X', 'B', '2', '4')
#define DRM_FORMAT_ABGR2101010 FOURCC('A', 'B', '3', '0')
#define DRM_FORMAT_ARGB2101010 FOURCC('A', 'R', '3', '0')
#define DRM_FORMAT_YVU420 FOURCC('Y', 'V', '1', '2')
#define DRM_FORMAT_NV12 FOURCC('N', 'V', '1', '2')
#define DRM_FORMAT_P010 FOURCC('P', '0', '1', '0')

namespace gl {
namespace {

// Returns corresponding internalformat if supported, and GL_NONE otherwise.
unsigned GLInternalFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::P010:
      return GL_RGB_YCBCR_P010_CHROMIUM;
    default:
      break;
  }
  return gl::BufferFormatToGLInternalFormat(format);
}

EGLint FourCC(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return DRM_FORMAT_R8;
    case gfx::BufferFormat::R_16:
      return DRM_FORMAT_R16;
    case gfx::BufferFormat::RG_88:
      return DRM_FORMAT_GR88;
    case gfx::BufferFormat::RG_1616:
      return DRM_FORMAT_GR1616;
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
    case gfx::BufferFormat::RGBA_1010102:
      return DRM_FORMAT_ABGR2101010;
    case gfx::BufferFormat::BGRA_1010102:
      return DRM_FORMAT_ARGB2101010;
    case gfx::BufferFormat::YVU_420:
      return DRM_FORMAT_YVU420;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return DRM_FORMAT_NV12;
    case gfx::BufferFormat::P010:
      return DRM_FORMAT_P010;
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::RGBA_F16:
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
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
      return gfx::BufferFormat::RGBA_1010102;
    case DRM_FORMAT_ARGB2101010:
      return gfx::BufferFormat::BGRA_1010102;
    case DRM_FORMAT_RGB565:
      return gfx::BufferFormat::BGR_565;
    case DRM_FORMAT_NV12:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    case DRM_FORMAT_YVU420:
      return gfx::BufferFormat::YVU_420;
    case DRM_FORMAT_P010:
      return gfx::BufferFormat::P010;
    default:
      NOTREACHED();
      return gfx::BufferFormat::BGRA_8888;
  }
}

}  // namespace

scoped_refptr<GLImageNativePixmap> GLImageNativePixmap::Create(
    const gfx::Size& size,
    gfx::BufferFormat format,
    scoped_refptr<gfx::NativePixmap> pixmap) {
  return CreateForPlane(size, format, gfx::BufferPlane::DEFAULT,
                        std::move(pixmap), gfx::ColorSpace());
}

scoped_refptr<GLImageNativePixmap> GLImageNativePixmap::CreateForPlane(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferPlane plane,
    scoped_refptr<gfx::NativePixmap> pixmap,
    const gfx::ColorSpace& color_space) {
  auto image =
      base::WrapRefCounted(new GLImageNativePixmap(size, format, plane));
  if (!image->Initialize(std::move(pixmap), color_space)) {
    return nullptr;
  }
  return image;
}

scoped_refptr<GLImageNativePixmap> GLImageNativePixmap::CreateFromTexture(
    const gfx::Size& size,
    gfx::BufferFormat format,
    uint32_t texture_id) {
  auto image = base::WrapRefCounted(
      new GLImageNativePixmap(size, format, gfx::BufferPlane::DEFAULT));
  if (!image->InitializeFromTexture(texture_id)) {
    return nullptr;
  }
  return image;
}

GLImageNativePixmap::GLImageNativePixmap(const gfx::Size& size,
                                         gfx::BufferFormat format,
                                         gfx::BufferPlane plane)
    : GLImageEGL(size),
      format_(format),
      plane_(plane),
      has_image_dma_buf_export_(gl::GLSurfaceEGL::GetGLDisplayEGL()
                                    ->ext->b_EGL_MESA_image_dma_buf_export) {}

GLImageNativePixmap::~GLImageNativePixmap() {}

bool GLImageNativePixmap::Initialize(scoped_refptr<gfx::NativePixmap> pixmap,
                                     const gfx::ColorSpace& color_space) {
  DCHECK(!pixmap_);
  if (GLInternalFormat(format_) == GL_NONE) {
    LOG(ERROR) << "Unsupported format: " << gfx::BufferFormatToString(format_);
    return false;
  }
  if (pixmap->AreDmaBufFdsValid()) {
    // Note: If eglCreateImageKHR is successful for a EGL_LINUX_DMA_BUF_EXT
    // target, the EGL will take a reference to the dma_buf.
    std::vector<EGLint> attrs;
    attrs.push_back(EGL_WIDTH);
    attrs.push_back(size_.width());
    attrs.push_back(EGL_HEIGHT);
    attrs.push_back(size_.height());
    attrs.push_back(EGL_LINUX_DRM_FOURCC_EXT);
    attrs.push_back(FourCC(format_));

    if (format_ == gfx::BufferFormat::YUV_420_BIPLANAR ||
        format_ == gfx::BufferFormat::YVU_420) {
      // TODO(b/233667677): Since https://crrev.com/c/3855381, the only NV12
      // quads that we allow to be promoted to overlays are those that don't use
      // the BT.2020 primaries and that don't use full range. Furthermore, since
      // https://crrev.com/c/2336347, we force the DRM/KMS driver to use BT.601
      // with limited range. Therefore, for compositing purposes, we need to a)
      // use EGL_ITU_REC601_EXT for any video frames that might be promoted to
      // overlays - we shouldn't use EGL_ITU_REC709_EXT because we might then
      // see a slight difference in compositing vs. overlays (note that the
      // BT.601 and BT.709 primaries are close to each other, so this shouldn't
      // be a huge correctness issue, though we'll need to address this at some
      // point); b) use EGL_ITU_REC2020_EXT for BT.2020 frames in order to
      // composite them correctly (and we won't need to worry about a difference
      // in compositing vs. overlays in this case since those frames won't be
      // promoted to overlays). We'll need to revisit this once we plumb the
      // color space and range to DRM/KMS.
      attrs.push_back(EGL_YUV_COLOR_SPACE_HINT_EXT);
      switch (color_space.GetMatrixID()) {
        case gfx::ColorSpace::MatrixID::BT2020_NCL:
          attrs.push_back(EGL_ITU_REC2020_EXT);
          break;
        default:
          attrs.push_back(EGL_ITU_REC601_EXT);
      }

      attrs.push_back(EGL_SAMPLE_RANGE_HINT_EXT);
      switch (color_space.GetRangeID()) {
        case gfx::ColorSpace::RangeID::FULL:
          attrs.push_back(EGL_YUV_FULL_RANGE_EXT);
          break;
        default:
          attrs.push_back(EGL_YUV_NARROW_RANGE_EXT);
      }
    }

    if (plane_ == gfx::BufferPlane::DEFAULT) {
      const EGLint kLinuxDrmModifiers[] = {EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
                                           EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
                                           EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT};
      bool has_dma_buf_import_modifier =
          gl::GLSurfaceEGL::GetGLDisplayEGL()
              ->ext->b_EGL_EXT_image_dma_buf_import_modifiers;

      for (size_t attrs_plane = 0; attrs_plane < pixmap->GetNumberOfPlanes();
           ++attrs_plane) {
        attrs.push_back(EGL_DMA_BUF_PLANE0_FD_EXT + attrs_plane * 3);

        size_t pixmap_plane = attrs_plane;

        attrs.push_back(pixmap->GetDmaBufFd(pixmap_plane));
        attrs.push_back(EGL_DMA_BUF_PLANE0_OFFSET_EXT + attrs_plane * 3);
        attrs.push_back(pixmap->GetDmaBufOffset(pixmap_plane));
        attrs.push_back(EGL_DMA_BUF_PLANE0_PITCH_EXT + attrs_plane * 3);
        attrs.push_back(pixmap->GetDmaBufPitch(pixmap_plane));
        uint64_t modifier = pixmap->GetBufferFormatModifier();
        if (has_dma_buf_import_modifier &&
            modifier != gfx::NativePixmapHandle::kNoModifier) {
          DCHECK(attrs_plane < std::size(kLinuxDrmModifiers));
          attrs.push_back(kLinuxDrmModifiers[attrs_plane]);
          attrs.push_back(modifier & 0xffffffff);
          attrs.push_back(kLinuxDrmModifiers[attrs_plane] + 1);
          attrs.push_back(static_cast<uint32_t>(modifier >> 32));
        }
      }
      attrs.push_back(EGL_NONE);
    } else {
      DCHECK(plane_ == gfx::BufferPlane::Y || plane_ == gfx::BufferPlane::UV);
      size_t pixmap_plane = plane_ == gfx::BufferPlane::Y ? 0 : 1;

      attrs.push_back(EGL_DMA_BUF_PLANE0_FD_EXT);
      attrs.push_back(pixmap->GetDmaBufFd(pixmap_plane));
      attrs.push_back(EGL_DMA_BUF_PLANE0_OFFSET_EXT);
      attrs.push_back(pixmap->GetDmaBufOffset(pixmap_plane));
      attrs.push_back(EGL_DMA_BUF_PLANE0_PITCH_EXT);
      attrs.push_back(pixmap->GetDmaBufPitch(pixmap_plane));
      attrs.push_back(EGL_NONE);
    }

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
  if (GLInternalFormat(format_) == GL_NONE) {
    LOG(ERROR) << "Unsupported format: " << gfx::BufferFormatToString(format_);
    return false;
  }
  GLContext* current_context = GLContext::GetCurrent();
  if (!current_context || !current_context->IsCurrent(nullptr)) {
    LOG(ERROR) << "No gl context bound to the current thread";
    return false;
  }

  EGLContext context_handle =
      reinterpret_cast<EGLContext>(current_context->GetHandle());
  DCHECK_NE(context_handle, EGL_NO_CONTEXT);

  if (!GLImageEGL::Initialize(context_handle, EGL_GL_TEXTURE_2D_KHR,
                              reinterpret_cast<EGLClientBuffer>(texture_id),
                              nullptr)) {
    return false;
  }
  return true;
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

  if (!eglExportDMABUFImageQueryMESA(
          GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(), egl_image_, &fourcc,
          &num_planes, &modifiers)) {
    LOG(ERROR) << "Error querying EGLImage: " << ui::GetLastEGLErrorString();
    return gfx::NativePixmapHandle();
  }

  if (num_planes <= 0 || num_planes > 4) {
    LOG(ERROR) << "Invalid number of planes: " << num_planes;
    return gfx::NativePixmapHandle();
  }

  gfx::BufferFormat format = GetBufferFormatFromFourCCFormat(fourcc);
  if (format != format_) {
    // A driver has returned a format different than what has been requested.
    // This can happen if RGBX is implemented using RGBA. Otherwise there is
    // a real mistake from the user and we have to fail.
    if (GetInternalFormat() == GL_RGB &&
        format != gfx::BufferFormat::RGBA_8888) {
      LOG(ERROR) << "Invalid driver format: "
                 << gfx::BufferFormatToString(format)
                 << " for requested format: "
                 << gfx::BufferFormatToString(format_);
      return gfx::NativePixmapHandle();
    }
  }

#if BUILDFLAG(IS_FUCHSIA)
  // TODO(crbug.com/852011): Implement image handle export on Fuchsia.
  NOTIMPLEMENTED();
  return gfx::NativePixmapHandle();
#else   // BUILDFLAG(IS_FUCHSIA)
  std::vector<int> fds(num_planes);
  std::vector<EGLint> strides(num_planes);
  std::vector<EGLint> offsets(num_planes);

  // It is specified for eglExportDMABUFImageMESA that the app is responsible
  // for closing any fds retrieved.
  if (!eglExportDMABUFImageMESA(GLSurfaceEGL::GetGLDisplayEGL()->GetDisplay(),
                                egl_image_, &fds[0], &strides[0],
                                &offsets[0])) {
    LOG(ERROR) << "Error exporting EGLImage: " << ui::GetLastEGLErrorString();
    return gfx::NativePixmapHandle();
  }

  gfx::NativePixmapHandle handle{};
  handle.modifier = modifiers;
  for (int i = 0; i < num_planes; ++i) {
    // Sanity check. In principle all the fds are meant to be valid when
    // eglExportDMABUFImageMESA succeeds.
    base::ScopedFD scoped_fd(fds[i]);
    if (!scoped_fd.is_valid()) {
      LOG(ERROR) << "Invalid dmabuf";
      return gfx::NativePixmapHandle();
    }

    handle.planes.emplace_back(strides[i], offsets[i], 0 /* size opaque */,
                               std::move(scoped_fd));
  }

  return handle;
#endif  // BUILDFLAG(IS_FUCHSIA)
}

unsigned GLImageNativePixmap::GetInternalFormat() {
  return GLInternalFormat(format_);
}

unsigned GLImageNativePixmap::GetDataType() {
  return gl::BufferFormatToGLDataType(format_);
}

bool GLImageNativePixmap::BindTexImage(unsigned target) {
  return GLImageEGL::BindTexImage(target);
}

bool GLImageNativePixmap::CopyTexImage(unsigned target) {
  if (egl_image_ != EGL_NO_IMAGE_KHR)
    return false;

  // Pass-through image type fails to bind and copy; make sure we
  // don't draw with uninitialized texture.
  std::vector<unsigned char> data(size_.width() * size_.height() * 4);
  glTexImage2D(target, 0, GL_RGBA, size_.width(), size_.height(), 0, GL_RGBA,
               GL_UNSIGNED_BYTE, data.data());
  return true;
}

bool GLImageNativePixmap::CopyTexSubImage(unsigned target,
                                          const gfx::Point& offset,
                                          const gfx::Rect& rect) {
  return false;
}

void GLImageNativePixmap::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    uint64_t process_tracing_id,
    const std::string& dump_name) {
  // TODO(ericrk): Implement GLImage OnMemoryDump. crbug.com/514914
}

scoped_refptr<gfx::NativePixmap> GLImageNativePixmap::GetNativePixmap() {
  return pixmap_;
}

}  // namespace gl
