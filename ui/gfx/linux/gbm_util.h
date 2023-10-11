// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_GBM_UTIL_H_
#define UI_GFX_LINUX_GBM_UTIL_H_

#include <cstdint>

#include "build/build_config.h"
#include "ui/gfx/buffer_types.h"

namespace ui {

// Get GBM buffer object usage flags for a corresponding gfx::BufferUsage.
// Depending on the platform, certain usage flags may not be available (eg.
// GBM_BO_USE_HW_VIDEO_ENCODER on desktop linux).
uint32_t BufferUsageToGbmFlags(gfx::BufferUsage usage);

#if BUILDFLAG(IS_CHROMEOS)
// If the environment variable, which determines whether minigbm should use
// Intel media compression [1], is not set, this function sets it based on the
// status of features::kEnableIntelMediaCompression. If it's already set, it
// only asserts that it's set consistently with
// features::kEnableIntelMediaCompression.
// [1]
// https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/kernel/v5.15/include/uapi/drm/drm_fourcc.h;l=569-580;drc=cc32a460b5fe0168bee851177e6071cbfdb15be1
//
// This function should not be called in a multi-threaded process, and it must
// be called after base::FeatureList is initialized and ready to be used. It may
// be called more than once.
void EnsureIntelMediaCompressionEnvVarIsSet();

// Returns true if the environment variable that controls Intel media
// compression in minigbm is set to either "0" or "1".
bool IntelMediaCompressionEnvVarIsSet();
#endif

}  // namespace ui

#endif  // UI_GFX_LINUX_GBM_UTIL_H_
