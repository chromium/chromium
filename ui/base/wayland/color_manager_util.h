// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WAYLAND_COLOR_MANAGER_UTIL_H_
#define UI_BASE_WAYLAND_COLOR_MANAGER_UTIL_H_

#include <chrome-color-management-server-protocol.h>

#include "base/containers/fixed_flat_map.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"

namespace ui::wayland {

// A map from the zcr_color_manager_v1 chromaticity_names enum values
// representing well-known chromaticities, to their equivalent PrimaryIDs.
// See components/exo/wayland/protocol/chrome-color-management.xml
constexpr auto kChromaticityMap =
    base::MakeFixedFlatMap<zcr_color_manager_v1_chromaticity_names,
                           gfx::ColorSpace::PrimaryID>(
        {{ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT601_525_LINE,
          gfx::ColorSpace::PrimaryID::SMPTE170M},
         {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT601_625_LINE,
          gfx::ColorSpace::PrimaryID::BT470BG},
         {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_SMPTE170M,
          gfx::ColorSpace::PrimaryID::SMPTE170M},
         {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT709,
          gfx::ColorSpace::PrimaryID::BT709},
         {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT2020,
          gfx::ColorSpace::PrimaryID::BT2020},
         {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_SRGB,
          gfx::ColorSpace::PrimaryID::BT709},
         {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_DISPLAYP3,
          gfx::ColorSpace::PrimaryID::P3},
         {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_ADOBERGB,
          gfx::ColorSpace::PrimaryID::ADOBE_RGB}});

// A map from the zcr_color_manager_v1 eotf_names enum values
// representing well-known EOTFs, to their equivalent TransferIDs.
// See components/exo/wayland/protocol/chrome-color-management.xml
constexpr auto kEotfMap =
    base::MakeFixedFlatMap<zcr_color_manager_v1_eotf_names,
                           gfx::ColorSpace::TransferID>({
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LINEAR,
         gfx::ColorSpace::TransferID::LINEAR},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB,
         gfx::ColorSpace::TransferID::BT709},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT2087,
         gfx::ColorSpace::TransferID::GAMMA24},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_ADOBERGB,
         // This is ever so slightly inaccurate. The number ought to be
         // 2.19921875f, not 2.2
         gfx::ColorSpace::TransferID::GAMMA22},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_PQ, gfx::ColorSpace::TransferID::PQ},
    });

zcr_color_manager_v1_chromaticity_names ToColorManagerChromaticity(
    gfx::ColorSpace::PrimaryID primaryID);

zcr_color_manager_v1_eotf_names ToColorManagerEOTF(
    gfx::ColorSpace::TransferID transferID);

}  // namespace ui::wayland

#endif  // UI_BASE_WAYLAND_COLOR_MANAGER_UTIL_H_