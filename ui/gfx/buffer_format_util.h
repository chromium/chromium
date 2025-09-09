// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BUFFER_FORMAT_UTIL_H_
#define UI_GFX_BUFFER_FORMAT_UTIL_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include <vulkan/vulkan_core.h>
#endif  // BUILDFLAG(ENABLE_VULKAN)

namespace gfx {

// Returns a span containing all buffer formats.
COMPONENT_EXPORT(GFX)
base::span<const BufferFormat> GetBufferFormatsForTesting();

// Returns the number of planes for |format|.
COMPONENT_EXPORT(GFX)
size_t NumberOfPlanesForLinearBufferFormat(BufferFormat format);

// Returns the subsampling factor applied to the given zero-indexed |plane| of
// |format| both horizontally and vertically.
COMPONENT_EXPORT(GFX)
size_t SubsamplingFactorForBufferFormat(BufferFormat format, size_t plane);

// Returns the alignment requirement to store a row of the given zero-indexed
// |plane| of |format|.
COMPONENT_EXPORT(GFX)
size_t RowByteAlignmentForBufferFormat(BufferFormat format, size_t plane);

// Returns the number of bytes used to store a row of the given zero-indexed
// |plane| of |format|.
COMPONENT_EXPORT(GFX)
size_t RowSizeForBufferFormat(size_t width, BufferFormat format, size_t plane);
[[nodiscard]] COMPONENT_EXPORT(GFX) bool RowSizeForBufferFormatChecked(
    size_t width,
    BufferFormat format,
    size_t plane,
    size_t* size_in_bytes);

// Returns the height in pixels of the given zero-indexed |plane| of |format|.
[[nodiscard]] COMPONENT_EXPORT(GFX) bool PlaneHeightForBufferFormatChecked(
    size_t width,
    BufferFormat format,
    size_t plane,
    size_t* height_in_pixels);

// Returns the number of bytes used to the plane of a given |format|.
COMPONENT_EXPORT(GFX)
size_t PlaneSizeForBufferFormat(const Size& size,
                                BufferFormat format,
                                size_t plane);
[[nodiscard]] COMPONENT_EXPORT(GFX) bool PlaneSizeForBufferFormatChecked(
    const Size& size,
    BufferFormat format,
    size_t plane,
    size_t* size_in_bytes);

// Returns the number of bytes used to store all the planes of a given |format|.
COMPONENT_EXPORT(GFX)
size_t BufferSizeForBufferFormat(const Size& size, BufferFormat format);

[[nodiscard]] COMPONENT_EXPORT(GFX) bool BufferSizeForBufferFormatChecked(
    const Size& size,
    BufferFormat format,
    size_t* size_in_bytes);

COMPONENT_EXPORT(GFX)
size_t BufferOffsetForBufferFormat(const Size& size,
                                   BufferFormat format,
                                   size_t plane);

// Returns the name of |format| as a string.
COMPONENT_EXPORT(GFX) const char* BufferFormatToString(BufferFormat format);

// Multiplanar buffer formats (e.g, YUV_420_BIPLANAR, YVU_420, P010) can be
// tricky when the size of the primary plane is odd, because the subsampled
// planes will have a size that is not a divisor of the primary plane's size.
// This indicates that odd height multiplanar formats are supported.
COMPONENT_EXPORT(GFX) bool IsOddHeightMultiPlanarBuffersAllowed();

COMPONENT_EXPORT(GFX) bool IsOddWidthMultiPlanarBuffersAllowed();

}  // namespace gfx

#endif  // UI_GFX_BUFFER_FORMAT_UTIL_H_
