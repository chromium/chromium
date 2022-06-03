// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_GBM_UTIL_H_
#define UI_GFX_LINUX_GBM_UTIL_H_

#include <cstdint>

#include "ui/gfx/buffer_types.h"

namespace ui {

// Get GBM buffer object usage flags for a corresponding gfx::BufferUsage.
// Depending on the platform, certain usage flags may not be available (eg.
// GBM_BO_USE_HW_VIDEO_ENCODER on desktop linux).
uint32_t BufferUsageToGbmFlags(gfx::BufferUsage usage);

}  // namespace ui

#endif  // UI_GFX_LINUX_GBM_UTIL_H_
