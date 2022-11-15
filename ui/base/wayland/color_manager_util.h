// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WAYLAND_COLOR_MANAGER_UTIL_H_
#define UI_BASE_WAYLAND_COLOR_MANAGER_UTIL_H_

#include <chrome-color-management-server-protocol.h>

#include "base/containers/fixed_flat_map.h"
#include "skia/ext/skcolorspace_trfn.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/display_color_spaces.h"

namespace ui::wayland {

// A 2.4 gamma for the BT2087 transfer function.
static constexpr skcms_TransferFunction gamma24 = {2.4f, 1.f};

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
         gfx::ColorSpace::TransferID::SRGB},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT709,
         gfx::ColorSpace::TransferID::BT709},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT2087,
         gfx::ColorSpace::TransferID::GAMMA24},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_ADOBERGB,
         // This is ever so slightly inaccurate. The number ought to be
         // 2.19921875f, not 2.2
         gfx::ColorSpace::TransferID::GAMMA22},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_PQ, gfx::ColorSpace::TransferID::PQ},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_HLG, gfx::ColorSpace::TransferID::HLG},
    });

// A map from the SDR zcr_color_manager_v1 eotf_names enum values
// representing well-known EOTFs, to their equivalent transfer functions.
// See components/exo/wayland/protocol/chrome-color-management.xml
constexpr auto kTransferMap =
    base::MakeFixedFlatMap<zcr_color_manager_v1_eotf_names,
                           skcms_TransferFunction>({
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LINEAR, SkNamedTransferFn::kLinear},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB, SkNamedTransferFnExt::kSRGB},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT709, SkNamedTransferFnExt::kRec709},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT2087, gamma24},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_ADOBERGB,
         SkNamedTransferFnExt::kA98RGB},
    });

// A map from the HDR zcr_color_manager_v1 eotf_names enum values
// representing well-known EOTFs, to their equivalent transfer functions.
// See components/exo/wayland/protocol/chrome-color-management.xml
constexpr auto kHDRTransferMap =
    base::MakeFixedFlatMap<zcr_color_manager_v1_eotf_names,
                           skcms_TransferFunction>(
        {{ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LINEAR, SkNamedTransferFn::kLinear},
         {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB, SkNamedTransferFnExt::kSRGB},
         {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_PQ, SkNamedTransferFn::kPQ},
         {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_HLG, SkNamedTransferFn::kHLG},
         {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_EXTENDEDSRGB10,
          SkNamedTransferFnExt::kSRGBExtended1023Over510}});

zcr_color_manager_v1_chromaticity_names ToColorManagerChromaticity(
    gfx::ColorSpace::PrimaryID primaryID);

zcr_color_manager_v1_eotf_names ToColorManagerEOTF(
    gfx::ColorSpace::TransferID transferID);

zcr_color_manager_v1_eotf_names ToColorManagerEOTF(gfx::ColorSpace color_space);

}  // namespace ui::wayland

#endif  // UI_BASE_WAYLAND_COLOR_MANAGER_UTIL_H_