// Copyright 2015 The Chromium Authors. All rights reserved.
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
GFX_EXPORT bool RowSizeForBufferFormatChecked(size_t width,
                                              BufferFormat format,
                                              size_t plane,
                                              size_t* size_in_bytes)
    WARN_UNUSED_RESULT;

// Returns the number of bytes used to store all the planes of a given |format|.
GFX_EXPORT size_t BufferSizeForBufferFormat(const Size& size,
                                            BufferFormat format);
GFX_EXPORT bool BufferSizeForBufferFormatChecked(const Size& size,
                                                 BufferFormat format,
                                                 size_t* size_in_bytes)
    WARN_UNUSED_RESULT;

GFX_EXPORT size_t BufferOffsetForBufferFormat(const Size& size,
                                           BufferFormat format,
                                           size_t plane);

// Returns the name of |format| as a string.
GFX_EXPORT const char* BufferFormatToString(BufferFormat format);

}  // namespace gfx

#endif  // UI_GFX_BUFFER_FORMAT_UTIL_H_
