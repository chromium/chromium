// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_BUFFER_FORMAT_UTIL_H_
#define UI_GFX_BUFFER_FORMAT_UTIL_H_

#include <stddef.h>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {

// Returns a span containing all buffer formats.
COMPONENT_EXPORT(GFX)
base::span<const BufferFormat> GetBufferFormatsForTesting();

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
