// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/buffer_format_util.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/gfx/switches.h"

namespace gfx {
namespace {

constexpr auto kBufferFormats = std::to_array<BufferFormat>(
    {BufferFormat::R_8, BufferFormat::R_16, BufferFormat::RG_88,
     BufferFormat::RG_1616, BufferFormat::BGR_565, BufferFormat::RGBA_4444,
     BufferFormat::RGBX_8888, BufferFormat::RGBA_8888, BufferFormat::BGRX_8888,
     BufferFormat::BGRA_1010102, BufferFormat::RGBA_1010102,
     BufferFormat::BGRA_8888, BufferFormat::RGBA_F16,
     BufferFormat::YUV_420_BIPLANAR, BufferFormat::YVU_420,
     BufferFormat::YUVA_420_TRIPLANAR, BufferFormat::P010});

static_assert(std::size(kBufferFormats) ==
                  (static_cast<int>(BufferFormat::LAST) + 1),
              "BufferFormat::LAST must be last value of kBufferFormats");

}  // namespace

base::span<const BufferFormat> GetBufferFormatsForTesting() {
  return kBufferFormats;
}

const char* BufferFormatToString(BufferFormat format) {
  switch (format) {
    case BufferFormat::R_8:
      return "R_8";
    case BufferFormat::R_16:
      return "R_16";
    case BufferFormat::RG_88:
      return "RG_88";
    case BufferFormat::RG_1616:
      return "RG_1616";
    case BufferFormat::BGR_565:
      return "BGR_565";
    case BufferFormat::RGBA_4444:
      return "RGBA_4444";
    case BufferFormat::RGBX_8888:
      return "RGBX_8888";
    case BufferFormat::RGBA_8888:
      return "RGBA_8888";
    case BufferFormat::BGRX_8888:
      return "BGRX_8888";
    case BufferFormat::BGRA_1010102:
      return "BGRA_1010102";
    case BufferFormat::RGBA_1010102:
      return "RGBA_1010102";
    case BufferFormat::BGRA_8888:
      return "BGRA_8888";
    case BufferFormat::RGBA_F16:
      return "RGBA_F16";
    case BufferFormat::YVU_420:
      return "YVU_420";
    case BufferFormat::YUV_420_BIPLANAR:
      return "YUV_420_BIPLANAR";
    case BufferFormat::YUVA_420_TRIPLANAR:
      return "YUVA_420_TRIPLANAR";
    case BufferFormat::P010:
      return "P010";
  }
  NOTREACHED() << "Invalid BufferFormat: " << base::to_underlying(format);
}

}  // namespace gfx
