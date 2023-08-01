// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/buffer_format_util.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_math.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/gfx/switches.h"

namespace gfx {
namespace {

const BufferFormat kBufferFormats[] = {
    BufferFormat::R_8,          BufferFormat::R_16,
    BufferFormat::RG_88,        BufferFormat::RG_1616,
    BufferFormat::BGR_565,      BufferFormat::RGBA_4444,
    BufferFormat::RGBX_8888,    BufferFormat::RGBA_8888,
    BufferFormat::BGRX_8888,    BufferFormat::BGRA_1010102,
    BufferFormat::RGBA_1010102, BufferFormat::BGRA_8888,
    BufferFormat::RGBA_F16,     BufferFormat::YUV_420_BIPLANAR,
    BufferFormat::YVU_420,      BufferFormat::YUVA_420_TRIPLANAR,
    BufferFormat::P010};

static_assert(std::size(kBufferFormats) ==
                  (static_cast<int>(BufferFormat::LAST) + 1),
              "BufferFormat::LAST must be last value of kBufferFormats");

}  // namespace

std::vector<BufferFormat> GetBufferFormatsForTesting() {
  return std::vector<BufferFormat>(kBufferFormats,
                                   kBufferFormats + std::size(kBufferFormats));
}

size_t AlphaBitsForBufferFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::RGBA_4444:
      return 4;
    case BufferFormat::RGBA_8888:
      return 8;
    case BufferFormat::BGRA_1010102:
    case BufferFormat::RGBA_1010102:
      return 2;
    case BufferFormat::BGRA_8888:
    case BufferFormat::YUVA_420_TRIPLANAR:
      return 8;
    case BufferFormat::RGBA_F16:
      return 16;
    case BufferFormat::R_8:
    case BufferFormat::R_16:
    case BufferFormat::RG_88:
    case BufferFormat::RG_1616:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBX_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::YVU_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::P010:
      return 0;
  }
  NOTREACHED();
  return 0;
}

size_t NumberOfPlanesForLinearBufferFormat(BufferFormat format) {
  switch (format) {
    case BufferFormat::R_8:
    case BufferFormat::R_16:
    case BufferFormat::RG_88:
    case BufferFormat::RG_1616:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBX_8888:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::BGRA_1010102:
    case BufferFormat::RGBA_1010102:
    case BufferFormat::BGRA_8888:
    case BufferFormat::RGBA_F16:
      return 1;
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::P010:
      return 2;
    case BufferFormat::YVU_420:
    case BufferFormat::YUVA_420_TRIPLANAR:
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
    case BufferFormat::RG_1616:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBX_8888:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::BGRA_1010102:
    case BufferFormat::RGBA_1010102:
    case BufferFormat::BGRA_8888:
    case BufferFormat::RGBA_F16:
      return 1;
    case BufferFormat::YVU_420: {
      constexpr size_t factor[] = {1, 2, 2};
      DCHECK_LT(plane, std::size(factor));
      return factor[plane];
    }
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::P010: {
      constexpr size_t factor[] = {1, 2};
      DCHECK_LT(plane, std::size(factor));
      return factor[plane];
    }
    case BufferFormat::YUVA_420_TRIPLANAR: {
      constexpr size_t factor[] = {1, 2, 1};
      DCHECK_LT(plane, std::size(factor));
      return factor[plane];
    }
  }
  NOTREACHED();
  return 0;
}

base::CheckedNumeric<size_t> PlaneWidthForBufferFormatChecked(
    size_t width,
    BufferFormat format,
    size_t plane) {
  const size_t subsample = SubsamplingFactorForBufferFormat(format, plane);
  return base::CheckDiv(base::CheckAdd(width, base::CheckSub(subsample, 1)),
                        subsample);
}

base::CheckedNumeric<size_t> PlaneHeightForBufferFormatChecked(
    size_t height,
    BufferFormat format,
    size_t plane) {
  const size_t subsample = SubsamplingFactorForBufferFormat(format, plane);
  return base::CheckDiv(base::CheckAdd(height, base::CheckSub(subsample, 1)),
                        subsample);
}

size_t BytesPerPixelForBufferFormat(BufferFormat format, size_t plane) {
  switch (format) {
    case BufferFormat::R_8:
      return 1;
    case BufferFormat::R_16:
    case BufferFormat::RG_88:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
      return 2;
    case BufferFormat::RG_1616:
    case BufferFormat::BGRX_8888:
    case BufferFormat::BGRA_1010102:
    case BufferFormat::RGBA_1010102:
    case BufferFormat::RGBX_8888:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRA_8888:
      return 4;
    case BufferFormat::RGBA_F16:
      return 8;
    case BufferFormat::YVU_420:
      return 1;
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::YUVA_420_TRIPLANAR:
      return SubsamplingFactorForBufferFormat(format, plane);
    case BufferFormat::P010:
      return 2 * SubsamplingFactorForBufferFormat(format, plane);
  }
  NOTREACHED();
  return 0;
}

size_t RowByteAlignmentForBufferFormat(BufferFormat format, size_t plane) {
  switch (format) {
    case BufferFormat::R_8:
    case BufferFormat::R_16:
    case BufferFormat::RG_88:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RG_1616:
    case BufferFormat::BGRX_8888:
    case BufferFormat::BGRA_1010102:
    case BufferFormat::RGBA_1010102:
    case BufferFormat::RGBX_8888:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRA_8888:
      return 4;
    case BufferFormat::RGBA_F16:
      return 8;
    case BufferFormat::YVU_420:
      return 1;
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::YUVA_420_TRIPLANAR:
    case BufferFormat::P010:
      return BytesPerPixelForBufferFormat(format, plane);
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
  base::CheckedNumeric<size_t> checked_size =
      PlaneWidthForBufferFormatChecked(width, format, plane);
  checked_size *= BytesPerPixelForBufferFormat(format, plane);
  const size_t alignment = RowByteAlignmentForBufferFormat(format, plane);
  checked_size = (checked_size + alignment - 1) & ~(alignment - 1);
  if (!checked_size.IsValid())
    return false;

  *size_in_bytes = checked_size.ValueOrDie();
  return true;
}

size_t PlaneSizeForBufferFormat(const Size& size,
                                BufferFormat format,
                                size_t plane) {
  size_t plane_size = 0;
  bool valid =
      PlaneSizeForBufferFormatChecked(size, format, plane, &plane_size);
  DCHECK(valid);
  return plane_size;
}

bool PlaneSizeForBufferFormatChecked(const Size& size,
                                     BufferFormat format,
                                     size_t plane,
                                     size_t* size_in_bytes) {
  size_t row_size = 0;
  if (!RowSizeForBufferFormatChecked(base::checked_cast<size_t>(size.width()),
                                     format, plane, &row_size)) {
    return false;
  }
  base::CheckedNumeric<size_t> checked_plane_size = row_size;
  checked_plane_size *= PlaneHeightForBufferFormatChecked(
      base::checked_cast<size_t>(size.height()), format, plane);
  if (!checked_plane_size.IsValid())
    return false;

  *size_in_bytes = checked_plane_size.ValueOrDie();
  return true;
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
    size_t plane_size = 0;
    if (!PlaneSizeForBufferFormatChecked(size, format, i, &plane_size))
      return false;
    checked_size += plane_size;
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
    case BufferFormat::RG_1616:
    case BufferFormat::BGR_565:
    case BufferFormat::RGBA_4444:
    case BufferFormat::RGBX_8888:
    case BufferFormat::RGBA_8888:
    case BufferFormat::BGRX_8888:
    case BufferFormat::BGRA_1010102:
    case BufferFormat::RGBA_1010102:
    case BufferFormat::BGRA_8888:
    case BufferFormat::RGBA_F16:
      return 0;
    case BufferFormat::YVU_420:
    case BufferFormat::YUV_420_BIPLANAR:
    case BufferFormat::YUVA_420_TRIPLANAR:
    case BufferFormat::P010: {
      size_t offset = 0;
      for (size_t i = 0; i < plane; i++) {
        offset += PlaneSizeForBufferFormat(size, format, i);
      }
      return offset;
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
  return "Invalid Format";
}

const char* BufferPlaneToString(BufferPlane format) {
  switch (format) {
    case BufferPlane::DEFAULT:
      return "DEFAULT";
    case BufferPlane::Y:
      return "Y";
    case BufferPlane::UV:
      return "UV";
    case BufferPlane::U:
      return "U";
    case BufferPlane::V:
      return "V";
    case BufferPlane::A:
      return "A";
  }
  NOTREACHED() << "Invalid BufferPlane: " << base::to_underlying(format);
  return "Invalid Plane";
}

bool IsOddHeightMultiPlanarBuffersAllowed() {
  return base::FeatureList::IsEnabled(features::kOddHeightMultiPlanarBuffers);
}

bool IsOddWidthMultiPlanarBuffersAllowed() {
  return base::FeatureList::IsEnabled(features::kOddWidthMultiPlanarBuffers);
}

}  // namespace gfx
