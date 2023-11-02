// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gdi_util.h"

#include <stddef.h>

#include <algorithm>
#include <memory>

#include "skia/ext/skia_utils_win.h"

namespace gfx {

void CreateBitmapV4HeaderForARGB888(int width,
                                    int height,
                                    BITMAPV4HEADER* hdr) {
  // Because bmp v4 header is just an extension, we just create a v3 header and
  // copy the bits over to the v4 header.
  BITMAPINFOHEADER header_v3;
  skia::CreateBitmapHeaderForXRGB888(width, height, &header_v3);
  memset(hdr, 0, sizeof(BITMAPV4HEADER));
  memcpy(hdr, &header_v3, sizeof(BITMAPINFOHEADER));

  // Correct the size of the header and fill in the mask values.
  hdr->bV4Size = sizeof(BITMAPV4HEADER);
  hdr->bV4RedMask   = 0x00ff0000;
  hdr->bV4GreenMask = 0x0000ff00;
  hdr->bV4BlueMask  = 0x000000ff;
  hdr->bV4AlphaMask = 0xff000000;
}

float CalculatePageScale(HDC dc, int page_width, int page_height) {
  int dc_width = GetDeviceCaps(dc, HORZRES);
  int dc_height = GetDeviceCaps(dc, VERTRES);

  // If page fits DC - no scaling needed.
  if (dc_width >= page_width && dc_height >= page_height)
    return 1.0;

  float x_factor =
      static_cast<float>(dc_width) / static_cast<float>(page_width);
  float y_factor =
      static_cast<float>(dc_height) / static_cast<float>(page_height);
  return std::min(x_factor, y_factor);
}

// Apply scaling to the DC.
bool ScaleDC(HDC dc, float scale_factor) {
  SetGraphicsMode(dc, GM_ADVANCED);
  XFORM xform = {0};
  xform.eM11 = xform.eM22 = scale_factor;
  return !!ModifyWorldTransform(dc, &xform, MWT_LEFTMULTIPLY);
}

}  // namespace gfx
