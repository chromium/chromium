// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/buffer_format_util.h"

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/stl_util.h"

namespace gfx {
namespace {

const BufferFormat kBufferFormats[] = {
    BufferFormat::R_8,              BufferFormat::R_16,
    BufferFormat::RG_88,            BufferFormat::BGR_565,
    BufferFormat::RGBA_4444,        BufferFormat::RGBX_8888,
    BufferFormat::RGBA_8888,        BufferFormat::BGRX_8888,
    BufferFormat::BGRX_1010102,     BufferFormat::RGBX_1010102,
    BufferFormat::BGRA_8888,        BufferFormat::RGBA_F16,
    BufferFormat::YUV_420_BIPLANAR, BufferFormat::YVU_420,
    BufferFormat::P010};

static_assert(base::size(kBufferFormats) ==
                  (static_cast<int>(BufferFormat::LAST) + 1),
              "BufferFormat::LAST must be last value of kBufferFormats");

}  // namespace

std::vector<BufferFormat> GetBufferFormatsForTesting() {
  return std::vector<BufferFormat>(kBufferFormats,
                                   kBufferFormats + base::size(kBufferFormats));
}

size_t NumberOfPlanesForLinearBufferFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::R_8:
    case BufferFormat::R_16:
    case BufferFormat::RG_88:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBX_8888:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::BGRX_1010102:
    case BufferFormat::RGBX_1010102:
    case BufferFormat::BGRA_8888:
    case BufferFormat::RGBA_F16:
      return 1;
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::P010:
      return 2;
    case BufferFormat::YVU_420:
      return 3;
  }
  NOTREACHED();
  return 0;
}

size_t SubsamplingFactorForBufferFormat(BufferFormat format, size_t plane) {
  switch (format) {
    case BufferFormat::R_8:
    case BufferFormat::R_16:
    case BufferFormat::RG_88:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBX_8888:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::BGRX_1010102:
    case BufferFormat::RGBX_1010102:
    case BufferFormat::BGRA_8888:
    case BufferFormat::RGBA_F16:
      return 1;
    case BufferFormat::YVU_420: {
      static size_t factor[] = {1, 2, 2};
      DCHECK_LT(static_cast<size_t>(plane), base::size(factor));
      return factor[plane];
    }
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::P010: {
      static size_t factor[] = {1, 2};
      DCHECK_LT(static_cast<size_t>(plane), base::size(factor));
      return factor[plane];
    }
  }
  NOTREACHED();
  return 0;
}

size_t RowSizeForBufferFormat(size_t width, BufferFormat format, size_t plane) {
  size_t row_size = 0;
  bool valid = RowSizeForBufferFormatChecked(width, format, plane, &row_size);
  DCHECK(valid);
  return row_size;
}

bool RowSizeForBufferFormatChecked(size_t width,
                                   BufferFormat format,
                                   size_t plane,
                                   size_t* size_in_bytes) {
  base::CheckedNumeric<size_t> checked_size = width;
  switch (format) {
    case BufferFormat::R_8:
      checked_size += 3;
      if (!checked_size.IsValid())
        return false;
      *size_in_bytes = (checked_size & ~0x3).ValueOrDie();
      return true;
    case BufferFormat::R_16:
    case BufferFormat::RG_88:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
      checked_size *= 2;
      checked_size += 3;
      if (!checked_size.IsValid())
        return false;
      *size_in_bytes = (checked_size & ~0x3).ValueOrDie();
      return true;
    case BufferFormat::BGRX_8888:
    case BufferFormat::BGRX_1010102:
    case BufferFormat::RGBX_1010102:
    case BufferFormat::RGBX_8888:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRA_8888:
      checked_size *= 4;
      if (!checked_size.IsValid())
        return false;
      *size_in_bytes = checked_size.ValueOrDie();
      return true;
    case BufferFormat::RGBA_F16:
      checked_size *= 8;
      if (!checked_size.IsValid())
        return false;
      *size_in_bytes = checked_size.ValueOrDie();
      return true;
    case BufferFormat::YVU_420:
      DCHECK_EQ(0u, width % 2);
      *size_in_bytes = width / SubsamplingFactorForBufferFormat(format, plane);
      return true;
    case BufferFormat::YUV_420_BIPLANAR:
      DCHECK_EQ(width % 2, 0u);
      *size_in_bytes = width;
      return true;
    case BufferFormat::P010:
      DCHECK_EQ(width % 2, 0u);
      *size_in_bytes = 2 * width;
      return true;
  }
  NOTREACHED();
  return false;
}

size_t BufferSizeForBufferFormat(const Size& size, BufferFormat format) {
  size_t buffer_size = 0;
  bool valid = BufferSizeForBufferFormatChecked(size, format, &buffer_size);
  DCHECK(valid);
  return buffer_size;
}

bool BufferSizeForBufferFormatChecked(const Size& size,
                                      BufferFormat format,
                                      size_t* size_in_bytes) {
  base::CheckedNumeric<size_t> checked_size = 0;
  size_t num_planes = NumberOfPlanesForLinearBufferFormat(format);
  for (size_t i = 0; i < num_planes; ++i) {
    size_t row_size = 0;
    if (!RowSizeForBufferFormatChecked(size.width(), format, i, &row_size))
      return false;
    base::CheckedNumeric<size_t> checked_plane_size = row_size;
    checked_plane_size *= size.height() /
                          SubsamplingFactorForBufferFormat(format, i);
    if (!checked_plane_size.IsValid())
      return false;
    checked_size += checked_plane_size.ValueOrDie();
    if (!checked_size.IsValid())
      return false;
  }
  *size_in_bytes = checked_size.ValueOrDie();
  return true;
}

size_t BufferOffsetForBufferFormat(const Size& size,
                                   BufferFormat format,
                                   size_t plane) {
  DCHECK_LT(plane, gfx::NumberOfPlanesForLinearBufferFormat(format));
  switch (format) {
    case BufferFormat::R_8:
    case BufferFormat::R_16:
    case BufferFormat::RG_88:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBX_8888:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::BGRX_1010102:
    case BufferFormat::RGBX_1010102:
    case BufferFormat::BGRA_8888:
    case BufferFormat::RGBA_F16:
      return 0;
    case BufferFormat::YVU_420: {
      static size_t offset_in_2x2_sub_sampling_sizes[] = {0, 4, 5};
      DCHECK_LT(plane, base::size(offset_in_2x2_sub_sampling_sizes));
      return offset_in_2x2_sub_sampling_sizes[plane] * (size.width() / 2) *
             (size.height() / 2);
    }
    case gfx::BufferFormat::YUV_420_BIPLANAR: {
      static size_t offset_in_2x2_sub_sampling_sizes[] = {0, 4};
      DCHECK_LT(plane, base::size(offset_in_2x2_sub_sampling_sizes));
      return offset_in_2x2_sub_sampling_sizes[plane] * (size.width() / 2) *
             (size.height() / 2);
    }
    case BufferFormat::P010: {
      static size_t offset_in_2x2_sub_sampling_sizes[] = {0, 4};
      DCHECK_LT(plane, base::size(offset_in_2x2_sub_sampling_sizes));
      return 2 * offset_in_2x2_sub_sampling_sizes[plane] *
             (size.width() / 2 + size.height() / 2);
    }
  }
  NOTREACHED();
  return 0;
}

const char* BufferFormatToString(BufferFormat format) {
  switch (format) {
    case BufferFormat::R_8:
      return "R_8";
    case BufferFormat::R_16:
      return "R_16";
    case BufferFormat::RG_88:
      return "RG_88";
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
    case BufferFormat::BGRX_1010102:
      return "BGRX_1010102";
    case BufferFormat::RGBX_1010102:
      return "RGBX_1010102";
    case BufferFormat::BGRA_8888:
      return "BGRA_8888";
    case BufferFormat::RGBA_F16:
      return "RGBA_F16";
    case BufferFormat::YVU_420:
      return "YVU_420";
    case BufferFormat::YUV_420_BIPLANAR:
      return "YUV_420_BIPLANAR";
    case BufferFormat::P010:
      return "P010";
  }
  NOTREACHED()
      << "Invalid BufferFormat: "
      << static_cast<typename std::underlying_type<BufferFormat>::type>(format);
  return "Invalid Format";
}

}  // namespace gfx
