// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_WIN_H_
#define UI_GFX_COLOR_SPACE_WIN_H_

#include <d3d11.h>
#include <d3d9.h>

// Must be included after d3d headers, use #if to avoid lint errors.
#if 1
#include <DXGIType.h>
#endif

// Work around bug in this header by disabling the relevant warning for it.
// https://connect.microsoft.com/VisualStudio/feedback/details/911260/dxva2api-h-in-win8-sdk-triggers-c4201-with-w4
#pragma warning(push)
#pragma warning(disable : 4201)
#include <dxva2api.h>
#pragma warning(pop)

#include "ui/gfx/color_space.h"

namespace gfx {

class COLOR_SPACE_EXPORT ColorSpaceWin {
 public:
  static DXVA2_ExtendedFormat GetExtendedFormat(const ColorSpace& color_space);

  // Returns a DXGI_COLOR_SPACE value based on the primaries and transfer
  // function of |color_space|. If the color space's MatrixID is RGB, then the
  // returned color space is also RGB unless |force_yuv| is true in which case
  // it is a YUV color space.
  static DXGI_COLOR_SPACE_TYPE GetDXGIColorSpace(const ColorSpace& color_space,
                                                 bool force_yuv = false);

  // Get DXGI format for swap chain. This will default to 8-bit, but will use
  // 10-bit or half-float for HDR color spaces.
  static DXGI_FORMAT GetDXGIFormat(const gfx::ColorSpace& color_space);

  static D3D11_VIDEO_PROCESSOR_COLOR_SPACE GetD3D11ColorSpace(
      const ColorSpace& color_space);
};

}  // namespace gfx
#endif  // UI_GFX_COLOR_SPACE_WIN_H_
