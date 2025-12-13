// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/drm_util_linux.h"

#include <drm_fourcc.h>

#include "base/logging.h"
#include "base/notreached.h"
#include "components/viz/common/resources/shared_image_format.h"

namespace ui {

int GetFourCCFormatFromBufferFormat(gfx::BufferFormat format) {
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
    case gfx::BufferFormat::RGBA_4444:
      return DRM_FORMAT_INVALID;
    case gfx::BufferFormat::RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case gfx::BufferFormat::RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case gfx::BufferFormat::BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case gfx::BufferFormat::BGRX_8888:
      return DRM_FORMAT_XRGB8888;
    case gfx::BufferFormat::BGRA_1010102:
      return DRM_FORMAT_ARGB2101010;
    case gfx::BufferFormat::RGBA_1010102:
      return DRM_FORMAT_ABGR2101010;
    case gfx::BufferFormat::RGBA_F16:
      return DRM_FORMAT_ABGR16161616F;
    case gfx::BufferFormat::YVU_420:
      return DRM_FORMAT_YVU420;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return DRM_FORMAT_NV12;
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return DRM_FORMAT_INVALID;
    case gfx::BufferFormat::P010:
      return DRM_FORMAT_P010;
  }
  return DRM_FORMAT_INVALID;
}

int GetFourCCFormatFromSharedImageFormat(const viz::SharedImageFormat& format) {
  if (format == viz::SinglePlaneFormat::kR_8) {
    return DRM_FORMAT_R8;
  }
  if (format == viz::SinglePlaneFormat::kR_16) {
    return DRM_FORMAT_R16;
  }
  if (format == viz::SinglePlaneFormat::kRG_88) {
    return DRM_FORMAT_GR88;
  }
  if (format == viz::SinglePlaneFormat::kRG_1616) {
    return DRM_FORMAT_GR1616;
  }
  if (format == viz::SinglePlaneFormat::kBGR_565) {
    return DRM_FORMAT_RGB565;
  }
  if (format == viz::SinglePlaneFormat::kRGBA_4444) {
    return DRM_FORMAT_INVALID;
  }
  if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return DRM_FORMAT_ABGR8888;
  }
  if (format == viz::SinglePlaneFormat::kRGBX_8888) {
    return DRM_FORMAT_XBGR8888;
  }
  if (format == viz::SinglePlaneFormat::kBGRA_8888) {
    return DRM_FORMAT_ARGB8888;
  }
  if (format == viz::SinglePlaneFormat::kBGRX_8888) {
    return DRM_FORMAT_XRGB8888;
  }
  if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
    return DRM_FORMAT_ARGB2101010;
  }
  if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
    return DRM_FORMAT_ABGR2101010;
  }
  if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return DRM_FORMAT_ABGR16161616F;
  }
  if (format == viz::MultiPlaneFormat::kYV12) {
    return DRM_FORMAT_YVU420;
  }
  if (format == viz::MultiPlaneFormat::kNV12) {
    return DRM_FORMAT_NV12;
  }
  if (format == viz::MultiPlaneFormat::kP010) {
    return DRM_FORMAT_P010;
  }
  // Other formats are not supported.
  return DRM_FORMAT_INVALID;
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
    case DRM_FORMAT_ARGB2101010:
      return gfx::BufferFormat::BGRA_1010102;
    case DRM_FORMAT_ABGR2101010:
      return gfx::BufferFormat::RGBA_1010102;
    case DRM_FORMAT_RGB565:
      return gfx::BufferFormat::BGR_565;
    case DRM_FORMAT_NV12:
      return gfx::BufferFormat::YUV_420_BIPLANAR;
    case DRM_FORMAT_YVU420:
      return gfx::BufferFormat::YVU_420;
    case DRM_FORMAT_P010:
      return gfx::BufferFormat::P010;
    case DRM_FORMAT_ABGR16161616F:
      return gfx::BufferFormat::RGBA_F16;
    default:
      NOTREACHED();
  }
}

viz::SharedImageFormat GetSharedImageFormatFromFourCCFormat(int format) {
  switch (format) {
    case DRM_FORMAT_R8:
      return viz::SinglePlaneFormat::kR_8;
    case DRM_FORMAT_GR88:
      return viz::SinglePlaneFormat::kRG_88;
    case DRM_FORMAT_ABGR8888:
      return viz::SinglePlaneFormat::kRGBA_8888;
    case DRM_FORMAT_XBGR8888:
      return viz::SinglePlaneFormat::kRGBX_8888;
    case DRM_FORMAT_ARGB8888:
      return viz::SinglePlaneFormat::kBGRA_8888;
    case DRM_FORMAT_XRGB8888:
      return viz::SinglePlaneFormat::kBGRX_8888;
    case DRM_FORMAT_ARGB2101010:
      return viz::SinglePlaneFormat::kBGRA_1010102;
    case DRM_FORMAT_ABGR2101010:
      return viz::SinglePlaneFormat::kRGBA_1010102;
    case DRM_FORMAT_RGB565:
      return viz::SinglePlaneFormat::kBGR_565;
    case DRM_FORMAT_NV12:
      return viz::MultiPlaneFormat::kNV12;
    case DRM_FORMAT_YVU420:
      return viz::MultiPlaneFormat::kYV12;
    case DRM_FORMAT_P010:
      return viz::MultiPlaneFormat::kP010;
    case DRM_FORMAT_ABGR16161616F:
      return viz::SinglePlaneFormat::kRGBA_F16;
    default:
      NOTREACHED();
  }
}

bool IsValidBufferFormat(uint32_t current_format) {
  switch (current_format) {
    case DRM_FORMAT_R8:
    case DRM_FORMAT_GR88:
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888:
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
    case DRM_FORMAT_ARGB2101010:
    case DRM_FORMAT_ABGR2101010:
    case DRM_FORMAT_RGB565:
    case DRM_FORMAT_NV12:
    case DRM_FORMAT_YVU420:
    case DRM_FORMAT_P010:
    case DRM_FORMAT_ABGR16161616F:
      return true;
    default:
      return false;
  }
}

const char* DrmFormatToString(uint32_t format) {
#define STRINGIFY(V) \
  case V:            \
    return #V
  switch (format) {
    STRINGIFY(DRM_FORMAT_R8);
    STRINGIFY(DRM_FORMAT_GR88);
    STRINGIFY(DRM_FORMAT_ABGR8888);
    STRINGIFY(DRM_FORMAT_XBGR8888);
    STRINGIFY(DRM_FORMAT_ARGB8888);
    STRINGIFY(DRM_FORMAT_XRGB8888);
    STRINGIFY(DRM_FORMAT_ARGB2101010);
    STRINGIFY(DRM_FORMAT_ABGR2101010);
    STRINGIFY(DRM_FORMAT_RGB565);
    STRINGIFY(DRM_FORMAT_NV12);
    STRINGIFY(DRM_FORMAT_YVU420);
    STRINGIFY(DRM_FORMAT_P010);
    STRINGIFY(DRM_FORMAT_ABGR16161616F);
    case DRM_FORMAT_INVALID:  // fallthroughs
    default:
      return "DRM_FORMAT_INVALID";
  }
#undef STRINGIFY
}
}  // namespace ui
