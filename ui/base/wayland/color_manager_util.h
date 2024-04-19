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
constexpr uint32_t kDefaultSinceVersion = 1;

// These structs are used for holding ColorSpace enums, and the version of the
// color management protocol their support was introduced.
struct TransferFnVersion {
  skcms_TransferFunction transfer_fn;
  uint32_t version;
};

struct TransferVersion {
  gfx::ColorSpace::TransferID transfer;
  uint32_t version;
};

struct PrimaryVersion {
  gfx::ColorSpace::PrimaryID primary;
  uint32_t version;
};

struct MatrixVersion {
  gfx::ColorSpace::MatrixID matrix;
  uint32_t version;
};

struct RangeVersion {
  gfx::ColorSpace::RangeID range;
  uint32_t version;
};

// A map from the zcr_color_manager_v1 chromaticity_names enum values
// representing well-known chromaticities, to their equivalent PrimaryIDs.
// See components/exo/wayland/protocol/chrome-color-management.xml
constexpr auto kChromaticityMap = base::MakeFixedFlatMap<
    zcr_color_manager_v1_chromaticity_names,
    PrimaryVersion>(
    {{ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT601_525_LINE,
      PrimaryVersion(gfx::ColorSpace::PrimaryID::SMPTE170M,
                     kDefaultSinceVersion)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT601_625_LINE,
      PrimaryVersion(gfx::ColorSpace::PrimaryID::BT470BG,
                     kDefaultSinceVersion)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_SMPTE170M,
      PrimaryVersion(gfx::ColorSpace::PrimaryID::SMPTE170M,
                     kDefaultSinceVersion)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT709,
      PrimaryVersion(gfx::ColorSpace::PrimaryID::BT709, kDefaultSinceVersion)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT2020,
      PrimaryVersion(gfx::ColorSpace::PrimaryID::BT2020, kDefaultSinceVersion)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_SRGB,
      PrimaryVersion(gfx::ColorSpace::PrimaryID::BT709, kDefaultSinceVersion)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_DISPLAYP3,
      PrimaryVersion(gfx::ColorSpace::PrimaryID::P3, kDefaultSinceVersion)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_ADOBERGB,
      PrimaryVersion(gfx::ColorSpace::PrimaryID::ADOBE_RGB,
                     kDefaultSinceVersion)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_WIDE_GAMUT_COLOR_SPIN,
      PrimaryVersion(
          gfx::ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN,
          ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_WIDE_GAMUT_COLOR_SPIN_SINCE_VERSION)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT470M,
      PrimaryVersion(
          gfx::ColorSpace::PrimaryID::BT470M,
          ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_BT470M_SINCE_VERSION)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_SMPTE240M,
      PrimaryVersion(
          gfx::ColorSpace::PrimaryID::SMPTE240M,
          ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_SMPTE240M_SINCE_VERSION)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_XYZ_D50,
      PrimaryVersion(
          gfx::ColorSpace::PrimaryID::XYZ_D50,
          ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_XYZ_D50_SINCE_VERSION)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_SMPTEST428_1,
      PrimaryVersion(
          gfx::ColorSpace::PrimaryID::SMPTEST428_1,
          ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_SMPTEST428_1_SINCE_VERSION)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_SMPTEST431_2,
      PrimaryVersion(
          gfx::ColorSpace::PrimaryID::SMPTEST431_2,
          ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_SMPTEST431_2_SINCE_VERSION)},
     {ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_FILM,
      PrimaryVersion(
          gfx::ColorSpace::PrimaryID::FILM,
          ZCR_COLOR_MANAGER_V1_CHROMATICITY_NAMES_FILM_SINCE_VERSION)}});

// A map from the zcr_color_manager_v1 eotf_names enum values
// representing well-known EOTFs, to their equivalent TransferIDs.
// See components/exo/wayland/protocol/chrome-color-management.xml
constexpr auto kEotfMap = base::MakeFixedFlatMap<
    zcr_color_manager_v1_eotf_names,
    TransferVersion>({
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LINEAR,
     TransferVersion(gfx::ColorSpace::TransferID::LINEAR,
                     kDefaultSinceVersion)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB,
     TransferVersion(gfx::ColorSpace::TransferID::SRGB, kDefaultSinceVersion)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB_HDR,
     TransferVersion(gfx::ColorSpace::TransferID::SRGB_HDR,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB_HDR_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT709,
     TransferVersion(gfx::ColorSpace::TransferID::BT709,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT709_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT2087,
     TransferVersion(gfx::ColorSpace::TransferID::GAMMA24,
                     kDefaultSinceVersion)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_ADOBERGB,
     // This is ever so slightly inaccurate. The number ought to be
     // 2.19921875f, not 2.2
     TransferVersion(gfx::ColorSpace::TransferID::GAMMA22,
                     kDefaultSinceVersion)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_PQ,
     TransferVersion(gfx::ColorSpace::TransferID::PQ, kDefaultSinceVersion)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_HLG,
     TransferVersion(gfx::ColorSpace::TransferID::HLG,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_HLG_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SMPTE170M,
     TransferVersion(gfx::ColorSpace::TransferID::SMPTE170M,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SMPTE170M_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SMPTE240M,
     TransferVersion(gfx::ColorSpace::TransferID::SMPTE240M,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SMPTE240M_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SMPTEST428_1,
     TransferVersion(
         gfx::ColorSpace::TransferID::SMPTEST428_1,
         ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SMPTEST428_1_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LOG,
     TransferVersion(gfx::ColorSpace::TransferID::LOG,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LOG_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LOG_SQRT,
     TransferVersion(gfx::ColorSpace::TransferID::LOG_SQRT,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LOG_SQRT_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_IEC61966_2_4,
     TransferVersion(
         gfx::ColorSpace::TransferID::IEC61966_2_4,
         ZCR_COLOR_MANAGER_V1_EOTF_NAMES_IEC61966_2_4_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT1361_ECG,
     TransferVersion(gfx::ColorSpace::TransferID::BT1361_ECG,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT1361_ECG_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT2020_10,
     TransferVersion(gfx::ColorSpace::TransferID::BT2020_10,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT2020_10_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT2020_12,
     TransferVersion(gfx::ColorSpace::TransferID::BT2020_12,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT2020_12_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SCRGB_LINEAR_80_NITS,
     TransferVersion(
         gfx::ColorSpace::TransferID::SCRGB_LINEAR_80_NITS,
         ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SCRGB_LINEAR_80_NITS_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_GAMMA18,
     TransferVersion(gfx::ColorSpace::TransferID::GAMMA18,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_GAMMA18_SINCE_VERSION)},
    {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_GAMMA28,
     TransferVersion(gfx::ColorSpace::TransferID::GAMMA28,
                     ZCR_COLOR_MANAGER_V1_EOTF_NAMES_GAMMA28_SINCE_VERSION)},
});

// A map from the SDR zcr_color_manager_v1 eotf_names enum values
// representing well-known EOTFs, to their equivalent transfer functions.
// See components/exo/wayland/protocol/chrome-color-management.xml
constexpr auto kTransferMap =
    base::MakeFixedFlatMap<zcr_color_manager_v1_eotf_names, TransferFnVersion>({
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LINEAR,
         TransferFnVersion(SkNamedTransferFn::kLinear, kDefaultSinceVersion)},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB,
         TransferFnVersion(SkNamedTransferFnExt::kSRGB, kDefaultSinceVersion)},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT709,
         TransferFnVersion(
             SkNamedTransferFnExt::kRec709,
             ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT709_SINCE_VERSION)},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_BT2087,
         TransferFnVersion(gamma24, kDefaultSinceVersion)},
        {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_ADOBERGB,
         TransferFnVersion(SkNamedTransferFnExt::kA98RGB,
                           kDefaultSinceVersion)},
    });

// A map from the HDR zcr_color_manager_v1 eotf_names enum values
// representing well-known EOTFs, to their equivalent transfer functions.
// See components/exo/wayland/protocol/chrome-color-management.xml
constexpr auto kHDRTransferMap =
    base::MakeFixedFlatMap<zcr_color_manager_v1_eotf_names, TransferFnVersion>(
        {{ZCR_COLOR_MANAGER_V1_EOTF_NAMES_LINEAR,
          TransferFnVersion(SkNamedTransferFn::kLinear, kDefaultSinceVersion)},
         {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB_HDR,
          TransferFnVersion(
              SkNamedTransferFnExt::kSRGB,
              ZCR_COLOR_MANAGER_V1_EOTF_NAMES_SRGB_HDR_SINCE_VERSION)},
         {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_PQ,
          TransferFnVersion(SkNamedTransferFn::kPQ, kDefaultSinceVersion)},
         {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_HLG,
          TransferFnVersion(SkNamedTransferFn::kHLG,
                            ZCR_COLOR_MANAGER_V1_EOTF_NAMES_HLG_SINCE_VERSION)},
         {ZCR_COLOR_MANAGER_V1_EOTF_NAMES_EXTENDEDSRGB10,
          TransferFnVersion(
              SkNamedTransferFnExt::kSRGBExtended1023Over510,
              ZCR_COLOR_MANAGER_V1_EOTF_NAMES_EXTENDEDSRGB10_SINCE_VERSION)}});

// A map from zcr_color_manager_v1 matrix_names enum values to
// gfx::ColorSpace::MatrixIDs.
constexpr auto kMatrixMap =
    base::MakeFixedFlatMap<zcr_color_manager_v1_matrix_names, MatrixVersion>(
        {{ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_RGB,
          MatrixVersion(gfx::ColorSpace::MatrixID::RGB, kDefaultSinceVersion)},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_BT709,
          MatrixVersion(gfx::ColorSpace::MatrixID::BT709,
                        kDefaultSinceVersion)},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_BT470BG,
          MatrixVersion(
              gfx::ColorSpace::MatrixID::BT470BG,
              ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_BT470BG_SINCE_VERSION)},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_BT2020_NCL,
          MatrixVersion(gfx::ColorSpace::MatrixID::BT2020_NCL,
                        kDefaultSinceVersion)},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_FCC,
          MatrixVersion(gfx::ColorSpace::MatrixID::FCC, kDefaultSinceVersion)},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_YCOCG,
          MatrixVersion(gfx::ColorSpace::MatrixID::YCOCG,
                        ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_YCOCG_SINCE_VERSION)},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_YDZDX,
          MatrixVersion(gfx::ColorSpace::MatrixID::YDZDX,
                        ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_YDZDX_SINCE_VERSION)},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_GBR,
          MatrixVersion(gfx::ColorSpace::MatrixID::GBR,
                        ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_GBR_SINCE_VERSION)},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_SMPTE170M,
          MatrixVersion(gfx::ColorSpace::MatrixID::SMPTE170M,
                        kDefaultSinceVersion)},
         {ZCR_COLOR_MANAGER_V1_MATRIX_NAMES_SMPTE240M,
          MatrixVersion(gfx::ColorSpace::MatrixID::SMPTE240M,
                        kDefaultSinceVersion)}});

// A map from zcr_color_manager_v1 range_names enum values to
// gfx::ColorSpace::RangeIDs.
constexpr auto kRangeMap =
    base::MakeFixedFlatMap<zcr_color_manager_v1_range_names, RangeVersion>(
        {{ZCR_COLOR_MANAGER_V1_RANGE_NAMES_LIMITED,
          RangeVersion(gfx::ColorSpace::RangeID::LIMITED,
                       kDefaultSinceVersion)},
         {ZCR_COLOR_MANAGER_V1_RANGE_NAMES_FULL,
          RangeVersion(gfx::ColorSpace::RangeID::FULL, kDefaultSinceVersion)},
         {ZCR_COLOR_MANAGER_V1_RANGE_NAMES_DERIVED,
          RangeVersion(gfx::ColorSpace::RangeID::DERIVED,
                       kDefaultSinceVersion)}});

zcr_color_manager_v1_chromaticity_names ToColorManagerChromaticity(
    gfx::ColorSpace::PrimaryID primaryID,
    uint32_t version);

zcr_color_manager_v1_matrix_names ToColorManagerMatrix(
    gfx::ColorSpace::MatrixID matrixID,
    uint32_t version);

zcr_color_manager_v1_range_names ToColorManagerRange(
    gfx::ColorSpace::RangeID rangeID,
    uint32_t version);

zcr_color_manager_v1_eotf_names ToColorManagerEOTF(
    gfx::ColorSpace::TransferID transferID,
    uint32_t version);

zcr_color_manager_v1_eotf_names ToColorManagerEOTF(gfx::ColorSpace color_space,
                                                   uint32_t version);

}  // namespace ui::wayland

#endif  // UI_BASE_WAYLAND_COLOR_MANAGER_UTIL_H_