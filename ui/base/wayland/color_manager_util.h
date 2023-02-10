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

// A map from zcr_color_manager_v1 matrix_names enum values to
// gfx::ColorSpace::MatrixIDs.
constexpr auto kMatrixMap =
    base::MakeFixedFlatMap<zcr_color_manager_v1_matrix_names,
                           gfx::ColorSpace::MatrixID>(
        {{ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_RGB,
          gfx::ColorSpace::MatrixID::RGB},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_BT709,
          gfx::ColorSpace::MatrixID::BT709},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_BT2020_NCL,
          gfx::ColorSpace::MatrixID::BT2020_NCL},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_BT2020_CL,
          gfx::ColorSpace::MatrixID::BT2020_CL},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_FCC,
          gfx::ColorSpace::MatrixID::FCC},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_SMPTE170M,
          gfx::ColorSpace::MatrixID::SMPTE170M},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_SMPTE240M,
          gfx::ColorSpace::MatrixID::SMPTE240M}});

// A map from zcr_color_manager_v1 range_names enum values to
// gfx::ColorSpace::RangeIDs.
constexpr auto kRangeMap =
    base::MakeFixedFlatMap<zcr_color_manager_v1_range_names,
                           gfx::ColorSpace::RangeID>(
        {{ZCR_COLOR_MANAGER_V1_RANGE_NAMES_LIMITED,
          gfx::ColorSpace::RangeID::LIMITED},
         {ZCR_COLOR_MANAGER_V1_RANGE_NAMES_FULL,
          gfx::ColorSpace::RangeID::FULL},
         {ZCR_COLOR_MANAGER_V1_RANGE_NAMES_DERIVED,
          gfx::ColorSpace::RangeID::DERIVED}});

zcr_color_manager_v1_chromaticity_names ToColorManagerChromaticity(
    gfx::ColorSpace::PrimaryID primaryID);

zcr_color_manager_v1_matrix_names ToColorManagerMatrix(
    gfx::ColorSpace::MatrixID matrixID);

zcr_color_manager_v1_range_names ToColorManagerRange(
    gfx::ColorSpace::RangeID rangeID);

zcr_color_manager_v1_eotf_names ToColorManagerEOTF(
    gfx::ColorSpace::TransferID transferID);

zcr_color_manager_v1_eotf_names ToColorManagerEOTF(gfx::ColorSpace color_space);

}  // namespace ui::wayland

#endif  // UI_BASE_WAYLAND_COLOR_MANAGER_UTIL_H_