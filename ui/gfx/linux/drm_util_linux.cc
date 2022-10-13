// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/linux/drm_util_linux.h"

#include <drm_fourcc.h>

#include <cctype>
#include <string>

#include "base/notreached.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"

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
      return DRM_FORMAT_INVALID;
    case gfx::BufferFormat::YVU_420:
      return DRM_FORMAT_YVU420;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return DRM_FORMAT_NV12;
    case gfx::BufferFormat::P010:
      return DRM_FORMAT_P010;
  }
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
    default:
      NOTREACHED();
      return gfx::BufferFormat::BGRA_8888;
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
      return true;
    default:
      return false;
  }
}

std::string DrmBufferFormatToString(uint32_t format) {
  constexpr char kInvalid[] = "INVALID";

  // DRM_FORMAT strings are structured as four meaningful chars packed into
  // a single int. If we didn't receive garbage, we should just return them (but
  // as a null-terminated string).
  char* format_chars = reinterpret_cast<char*>(&format);
  auto is_valid = [](char c) { return c == ' ' || isalnum(c); };
  if (!(is_valid(format_chars[0]) && is_valid(format_chars[1]) &&
        is_valid(format_chars[2]) && is_valid(format_chars[3]))) {
    return kInvalid;
  }

  // Trim, for example, 'R8  ' -> 'R8'
  std::string str(format_chars, 4);
  base::StringPiece trimmed =
      base::TrimWhitespaceASCII(str, base::TrimPositions::TRIM_ALL);
  if (trimmed.empty())
    return kInvalid;

  return std::string(trimmed);
}

}  // namespace ui
