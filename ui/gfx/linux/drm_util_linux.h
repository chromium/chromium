// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_LINUX_DRM_UTIL_LINUX_H_
#define UI_GFX_LINUX_DRM_UTIL_LINUX_H_

#include <cstdint>

#include "components/viz/common/resources/shared_image_format.h"

namespace ui {

inline constexpr auto kDrmSharedImageFormats = {
    viz::SinglePlaneFormat::kR_8,
    viz::SinglePlaneFormat::kRG_88,
    viz::SinglePlaneFormat::kR_16,
    viz::SinglePlaneFormat::kRG_1616,
    viz::SinglePlaneFormat::kBGR_565,
    viz::SinglePlaneFormat::kRGBA_4444,
    viz::SinglePlaneFormat::kRGBA_8888,
    viz::SinglePlaneFormat::kRGBX_8888,
    viz::SinglePlaneFormat::kBGRA_8888,
    viz::SinglePlaneFormat::kBGRX_8888,
    viz::SinglePlaneFormat::kRGBA_1010102,
    viz::SinglePlaneFormat::kBGRA_1010102,
    viz::SinglePlaneFormat::kRGBA_F16,
    viz::MultiPlaneFormat::kYV12,
    viz::MultiPlaneFormat::kNV12,
    viz::MultiPlaneFormat::kP010};

int GetFourCCFormatFromSharedImageFormat(const viz::SharedImageFormat& format);

viz::SharedImageFormat GetSharedImageFormatFromFourCCFormat(int format);

// Returns true if the DRM FourCC format is known.
bool IsValidDrmFormat(uint32_t current_format);

// Returns a human-readable string for a DRM FourCC format, or
// DRM_FORMAT_INVALID for an unknown or unsupported DRM format.
const char* DrmFormatToString(uint32_t format);

}  // namespace ui

#endif  // UI_GFX_LINUX_DRM_UTIL_LINUX_H_
