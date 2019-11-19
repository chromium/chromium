// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/buffer_format_utils.h"

#include "ui/gl/gl_bindings.h"

namespace gl {

unsigned BufferFormatToGLInternalFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return GL_RED_EXT;
    case gfx::BufferFormat::R_16:
      return GL_R16_EXT;
    case gfx::BufferFormat::RG_88:
      return GL_RG_EXT;
    case gfx::BufferFormat::BGR_565:
      return GL_RGB;
    case gfx::BufferFormat::RGBA_4444:
      return GL_RGBA;
    case gfx::BufferFormat::RGBX_8888:
      return GL_RGB;
    case gfx::BufferFormat::RGBA_8888:
      return GL_RGBA;
    case gfx::BufferFormat::BGRX_8888:
      return GL_RGB;
    case gfx::BufferFormat::BGRX_1010102:
      return GL_RGB10_A2_EXT;
    case gfx::BufferFormat::RGBX_1010102:
      return GL_RGB10_A2_EXT;
    case gfx::BufferFormat::BGRA_8888:
      return GL_BGRA_EXT;
    case gfx::BufferFormat::RGBA_F16:
      return GL_RGBA;
    case gfx::BufferFormat::YVU_420:
      return GL_RGB_YCRCB_420_CHROMIUM;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return GL_RGB_YCBCR_420V_CHROMIUM;
    case gfx::BufferFormat::P010:
      return GL_RGB_YCBCR_P010_CHROMIUM;
  }

  NOTREACHED();
  return GL_NONE;
}

unsigned BufferFormatToGLDataType(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::BGRA_8888:
      return GL_UNSIGNED_BYTE;
    case gfx::BufferFormat::R_16:
      return GL_UNSIGNED_SHORT;
    case gfx::BufferFormat::BGR_565:
      return GL_UNSIGNED_SHORT_5_6_5;
    case gfx::BufferFormat::RGBA_4444:
      return GL_UNSIGNED_SHORT_4_4_4_4;
    case gfx::BufferFormat::RGBX_1010102:
    case gfx::BufferFormat::BGRX_1010102:
      return GL_UNSIGNED_INT_2_10_10_10_REV;
    case gfx::BufferFormat::RGBA_F16:
      return GL_HALF_FLOAT_OES;
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::P010:
      return GL_NONE;
  }

  NOTREACHED();
  return GL_NONE;
}

}  // namespace gl
