// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BUFFER_FORMAT_UTIL_H_
#define UI_GFX_BUFFER_FORMAT_UTIL_H_

#include <stddef.h>

#include <vector>

#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

// Returns a vector containing all buffer formats.
GFX_EXPORT std::vector<BufferFormat> GetBufferFormatsForTesting();

// Returns the number of bits of alpha for the specified format.
GFX_EXPORT size_t AlphaBitsForBufferFormat(BufferFormat format);

// Returns the number of planes for |format|.
GFX_EXPORT size_t NumberOfPlanesForLinearBufferFormat(BufferFormat format);

// Returns the subsampling factor applied to the given zero-indexed |plane| of
// |format| both horizontally and vertically.
GFX_EXPORT size_t SubsamplingFactorForBufferFormat(BufferFormat format,
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

// Returns the name of |plane| as a string.
GFX_EXPORT const char* BufferPlaneToString(BufferPlane plane);

// Multiplanar buffer formats (e.g, YUV_420_BIPLANAR, YVU_420, P010) can be
// tricky when the size of the primary plane is odd, because the subsampled
// planes will have a size that is not a divisor of the primary plane's size.
// This indicates that odd height multiplanar formats are supported.
GFX_EXPORT bool IsOddHeightMultiPlanarBuffersAllowed();

GFX_EXPORT bool IsOddWidthMultiPlanarBuffersAllowed();
}  // namespace gfx

#endif  // UI_GFX_BUFFER_FORMAT_UTIL_H_
