// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/common/native_pixmap_egl_binding.h"

#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/scoped_binders.h"

namespace ui {

namespace {

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
#define DRM_FORMAT_ABGR16161616F FOURCC('A', 'B', '4', 'H')
#define DRM_FORMAT_YVU420 FOURCC('Y', 'V', '1', '2')
#define DRM_FORMAT_NV12 FOURCC('N', 'V', '1', '2')
#define DRM_FORMAT_P010 FOURCC('P', '0', '1', '0')
/* Reserve 0 for the invalid format specifier */
#define DRM_FORMAT_INVALID 0

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
    case gfx::BufferFormat::RGBA_F16:
      return DRM_FORMAT_ABGR16161616F;
    case gfx::BufferFormat::RGBA_4444:
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return DRM_FORMAT_INVALID;
  }

  NOTREACHED_IN_MIGRATION();
  return DRM_FORMAT_INVALID;
}

}  // namespace

NativePixmapEGLBinding::NativePixmapEGLBinding(const gfx::Size& size,
                                               gfx::BufferFormat format,
                                               gfx::BufferPlane plane)
    : size_(size), format_(format), plane_(plane) {}

NativePixmapEGLBinding::~NativePixmapEGLBinding() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool NativePixmapEGLBinding::IsBufferFormatSupported(gfx::BufferFormat format) {
  return FourCC(format) != DRM_FORMAT_INVALID;
}

// static
std::unique_ptr<NativePixmapGLBinding> NativePixmapEGLBinding::Create(
    scoped_refptr<gfx::NativePixmap> pixmap,
    gfx::BufferFormat plane_format,
    gfx::BufferPlane plane,
    gfx::Size plane_size,
    const gfx::ColorSpace& color_space,
    GLenum target,
    GLuint texture_id) {
  DCHECK_GT(texture_id, 0u);

  auto binding =
      std::make_unique<NativePixmapEGLBinding>(plane_size, plane_format, plane);

  if (!binding->InitializeFromNativePixmap(std::move(pixmap), color_space,
                                           target, texture_id)) {
    LOG(ERROR) << "Unable to initialize binding from pixmap";
    return nullptr;
  }
  return binding;
}

bool NativePixmapEGLBinding::InitializeFromNativePixmap(
    scoped_refptr<gfx::NativePixmap> pixmap,
    const gfx::ColorSpace& color_space,
    GLenum target,
    GLuint texture_id) {
  DCHECK(!pixmap_);
  if (FourCC(format_) == DRM_FORMAT_INVALID) {
    LOG(ERROR) << "Unsupported format: " << gfx::BufferFormatToString(format_);
    return false;
  }

  if (!pixmap->AreDmaBufFdsValid()) {
    LOG(ERROR) << "Pixmap doesn't have valid dma bufs";
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
  attrs.push_back(FourCC(format_));

  if (format_ == gfx::BufferFormat::YUV_420_BIPLANAR ||
      format_ == gfx::BufferFormat::YVU_420 ||
      format_ == gfx::BufferFormat::P010) {
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
    constexpr EGLint kPlaneFDAttrs[] = {
        EGL_DMA_BUF_PLANE0_FD_EXT,
        EGL_DMA_BUF_PLANE1_FD_EXT,
        EGL_DMA_BUF_PLANE2_FD_EXT,
        EGL_DMA_BUF_PLANE3_FD_EXT,
    };
    constexpr EGLint kPlaneOffsetAttrs[] = {
        EGL_DMA_BUF_PLANE0_OFFSET_EXT,
        EGL_DMA_BUF_PLANE1_OFFSET_EXT,
        EGL_DMA_BUF_PLANE2_OFFSET_EXT,
        EGL_DMA_BUF_PLANE3_OFFSET_EXT,
    };

    constexpr EGLint kPlanePitchAttrs[] = {
        EGL_DMA_BUF_PLANE0_PITCH_EXT,
        EGL_DMA_BUF_PLANE1_PITCH_EXT,
        EGL_DMA_BUF_PLANE2_PITCH_EXT,
        EGL_DMA_BUF_PLANE3_PITCH_EXT,
    };

    constexpr EGLint kPlaneLoModifierAttrs[] = {
        EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT,
        EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT,
    };

    constexpr EGLint kPlaneHiModifierAttrs[] = {
        EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT,
        EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT,
    };

    bool has_dma_buf_import_modifier =
        gl::GLSurfaceEGL::GetGLDisplayEGL()
            ->ext->b_EGL_EXT_image_dma_buf_import_modifiers;
    CHECK_LE(pixmap->GetNumberOfPlanes(), std::size(kPlaneFDAttrs));
    for (size_t attrs_plane = 0; attrs_plane < pixmap->GetNumberOfPlanes();
         ++attrs_plane) {
      attrs.push_back(kPlaneFDAttrs[attrs_plane]);
      attrs.push_back(pixmap->GetDmaBufFd(attrs_plane));
      attrs.push_back(kPlaneOffsetAttrs[attrs_plane]);
      attrs.push_back(pixmap->GetDmaBufOffset(attrs_plane));
      attrs.push_back(kPlanePitchAttrs[attrs_plane]);
      attrs.push_back(pixmap->GetDmaBufPitch(attrs_plane));

      uint64_t modifier = pixmap->GetBufferFormatModifier();
      if (has_dma_buf_import_modifier &&
          modifier != gfx::NativePixmapHandle::kNoModifier) {
        DCHECK(attrs_plane < std::size(kPlaneLoModifierAttrs));
        DCHECK(attrs_plane < std::size(kPlaneHiModifierAttrs));
        attrs.push_back(kPlaneLoModifierAttrs[attrs_plane]);
        attrs.push_back(modifier & 0xffffffff);
        attrs.push_back(kPlaneHiModifierAttrs[attrs_plane]);
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

  egl_image_ =
      gl::MakeScopedEGLImage(EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                             static_cast<EGLClientBuffer>(nullptr), &attrs[0]);
  if (!egl_image_.get()) {
    return false;
  }

  pixmap_ = pixmap;

  gl::ScopedTextureBinder binder(target, texture_id);
  glEGLImageTargetTexture2DOES(target, egl_image_.get());

  return true;
}

}  // namespace ui
