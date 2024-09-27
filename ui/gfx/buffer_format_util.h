// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BUFFER_FORMAT_UTIL_H_
#define UI_GFX_BUFFER_FORMAT_UTIL_H_

#include <stddef.h>

#include <vector>

#include "base/containers/span.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include <vulkan/vulkan_core.h>
#endif  // BUILDFLAG(ENABLE_VULKAN)

namespace gfx {

// Returns a span containing all buffer formats.
GFX_EXPORT base::span<const BufferFormat> GetBufferFormatsForTesting();

// Returns the number of bits of alpha for the specified format.
GFX_EXPORT size_t AlphaBitsForBufferFormat(BufferFormat format);

// Returns the number of planes for |format|.
GFX_EXPORT size_t NumberOfPlanesForLinearBufferFormat(BufferFormat format);

// Returns whether |format| is multiplanar.
GFX_EXPORT bool BufferFormatIsMultiplanar(BufferFormat format);

// Returns the subsampling factor applied to the given zero-indexed |plane| of
// |format| both horizontally and vertically.
GFX_EXPORT size_t SubsamplingFactorForBufferFormat(BufferFormat format,
                                                   size_t plane);

// Returns the alignment requirement to store a row of the given zero-indexed
// |plane| of |format|.
GFX_EXPORT size_t RowByteAlignmentForBufferFormat(BufferFormat format,
                                                  size_t plane);

// Returns the number of bytes used to store a row of the given zero-indexed
// |plane| of |format|.
GFX_EXPORT size_t RowSizeForBufferFormat(size_t width,
                                         BufferFormat format,
                                         size_t plane);
[[nodiscard]] GFX_EXPORT bool RowSizeForBufferFormatChecked(
    size_t width,
    BufferFormat format,
    size_t plane,
    size_t* size_in_bytes);

// Returns the height in pixels of the given zero-indexed |plane| of |format|.
[[nodiscard]] GFX_EXPORT bool PlaneHeightForBufferFormatChecked(
    size_t width,
    BufferFormat format,
    size_t plane,
    size_t* height_in_pixels);

// Returns the number of bytes used to the plane of a given |format|.
GFX_EXPORT size_t PlaneSizeForBufferFormat(const Size& size,
                                           BufferFormat format,
                                           size_t plane);
[[nodiscard]] GFX_EXPORT bool PlaneSizeForBufferFormatChecked(
    const Size& size,
    BufferFormat format,
    size_t plane,
    size_t* size_in_bytes);

// Returns the number of bytes used to store all the planes of a given |format|.
GFX_EXPORT size_t BufferSizeForBufferFormat(const Size& size,
                                            BufferFormat format);

[[nodiscard]] GFX_EXPORT bool BufferSizeForBufferFormatChecked(
    const Size& size,
    BufferFormat format,
    size_t* size_in_bytes);

GFX_EXPORT size_t BufferOffsetForBufferFormat(const Size& size,
                                           BufferFormat format,
                                           size_t plane);

// Returns the name of |format| as a string.
GFX_EXPORT const char* BufferFormatToString(BufferFormat format);

// Multiplanar buffer formats (e.g, YUV_420_BIPLANAR, YVU_420, P010) can be
// tricky when the size of the primary plane is odd, because the subsampled
// planes will have a size that is not a divisor of the primary plane's size.
// This indicates that odd height multiplanar formats are supported.
GFX_EXPORT bool IsOddHeightMultiPlanarBuffersAllowed();

GFX_EXPORT bool IsOddWidthMultiPlanarBuffersAllowed();

#if BUILDFLAG(ENABLE_VULKAN)
// Converts a gfx::BufferFormat to its corresponding VkFormat.
GFX_EXPORT constexpr VkFormat ToVkFormat(const BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::BGRA_8888:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case gfx::BufferFormat::R_8:
      return VK_FORMAT_R8_UNORM;
    case gfx::BufferFormat::R_16:
      return VK_FORMAT_R16_UNORM;
    case gfx::BufferFormat::RG_1616:
      return VK_FORMAT_R16G16_UNORM;
    case gfx::BufferFormat::RGBA_4444:
      return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
    case gfx::BufferFormat::RGBA_8888:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case gfx::BufferFormat::RGBA_F16:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case gfx::BufferFormat::BGR_565:
      return VK_FORMAT_R5G6B5_UNORM_PACK16;
    case gfx::BufferFormat::RG_88:
      return VK_FORMAT_R8G8_UNORM;
    case gfx::BufferFormat::RGBX_8888:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case gfx::BufferFormat::BGRX_8888:
      return VK_FORMAT_B8G8R8A8_UNORM;
    case gfx::BufferFormat::RGBA_1010102:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case gfx::BufferFormat::BGRA_1010102:
      return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case gfx::BufferFormat::YVU_420:
      return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
    case gfx::BufferFormat::YUVA_420_TRIPLANAR:
      return VK_FORMAT_UNDEFINED;
    case gfx::BufferFormat::P010:
      return VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
  }
  return VK_FORMAT_UNDEFINED;
}
#endif  // BUILDFLAG(ENABLE_VULKAN)
}  // namespace gfx

#endif  // UI_GFX_BUFFER_FORMAT_UTIL_H_
