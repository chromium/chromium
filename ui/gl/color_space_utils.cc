// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/color_space_utils.h"

#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"

namespace gl {

// static
GLenum ColorSpaceUtils::GetGLColorSpace(const gfx::ColorSpace& color_space) {
  if (color_space.transfer_ == gfx::ColorSpace::TransferID::LINEAR_HDR)
    return GL_COLOR_SPACE_SCRGB_LINEAR_CHROMIUM;
  else if (color_space.transfer_ == gfx::ColorSpace::TransferID::SMPTEST2084)
    return GL_COLOR_SPACE_HDR10_CHROMIUM;
  else
    return GL_COLOR_SPACE_UNSPECIFIED_CHROMIUM;
}

// static
GLSurface::ColorSpace ColorSpaceUtils::GetGLSurfaceColorSpace(
    const gfx::ColorSpace& color_space) {
  if (color_space.transfer_ == gfx::ColorSpace::TransferID::LINEAR_HDR)
    return GLSurface::ColorSpace::SCRGB_LINEAR;
  else if (color_space.transfer_ == gfx::ColorSpace::TransferID::SMPTEST2084)
    return GLSurface::ColorSpace::HDR10;
  else
    return GLSurface::ColorSpace::UNSPECIFIED;
}

#if defined(OS_WIN)
DXGI_COLOR_SPACE_TYPE ColorSpaceUtils::GetDXGIColorSpace(
    GLSurface::ColorSpace color_space) {
  if (color_space == GLSurface::ColorSpace::SCRGB_LINEAR)
    return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
  else if (color_space == GLSurface::ColorSpace::HDR10)
    return DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
  else
    return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
}

DXGI_FORMAT ColorSpaceUtils::GetDXGIFormat(GLSurface::ColorSpace color_space) {
  if (color_space == GLSurface::ColorSpace::SCRGB_LINEAR)
    return DXGI_FORMAT_R16G16B16A16_FLOAT;
  else if (color_space == GLSurface::ColorSpace::HDR10)
    return DXGI_FORMAT_R10G10B10A2_UNORM;
  else
    return DXGI_FORMAT_B8G8R8A8_UNORM;
}
#endif  // OS_WIN

}  // namespace gl
