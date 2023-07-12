// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/native_pixmap_gl_binding.h"

#include "base/notreached.h"
#include "ui/gl/gl_bindings.h"

namespace ui {

NativePixmapGLBinding::NativePixmapGLBinding() = default;
NativePixmapGLBinding::~NativePixmapGLBinding() = default;

// static
unsigned NativePixmapGLBinding::BufferFormatToGLInternalFormatDefaultMapping(
    gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::R_8:
      return GL_RED_EXT;
    case gfx::BufferFormat::R_16:
      return GL_R16_EXT;
    case gfx::BufferFormat::RG_88:
      return GL_RG_EXT;
    case gfx::BufferFormat::RG_1616:
      return GL_RG16_EXT;
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
    case gfx::BufferFormat::BGRA_1010102:
      return GL_RGB10_A2_EXT;
    case gfx::BufferFormat::RGBA_1010102:
      return GL_RGB10_A2_EXT;
    case gfx::BufferFormat::BGRA_8888:
      return GL_BGRA_EXT;
    case gfx::BufferFormat::RGBA_F16:
      return GL_RGBA;
    case gfx::BufferFormat::YVU_420:
      return GL_RGB_YCRCB_420_CHROMIUM;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return GL_RGB_YCBCR_420V_CHROMIUM;
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return GL_NONE;
    case gfx::BufferFormat::P010:
      return GL_RGB_YCBCR_P010_CHROMIUM;
  }

  NOTREACHED();
  return GL_NONE;
}

}  // namespace ui
