// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_DRM_UTIL_LINUX_H_
#define UI_GFX_LINUX_DRM_UTIL_LINUX_H_

#include <cstdint>

#include "ui/gfx/buffer_types.h"

namespace ui {

int GetFourCCFormatFromBufferFormat(gfx::BufferFormat format);
gfx::BufferFormat GetBufferFormatFromFourCCFormat(int format);

// Returns true if the fourcc format is known.
bool IsValidBufferFormat(uint32_t current_format);

// Returns a human-readable string for a DRM FourCC format, or
// DRM_FORMAT_INVALID for an unknown or unsupported DRM format.
const char* DrmFormatToString(uint32_t format);

}  // namespace ui

#endif  // UI_GFX_LINUX_DRM_UTIL_LINUX_H__
