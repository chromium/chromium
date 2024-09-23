// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MOJOM_COLOR_SPACE_MOJOM_TRAITS_H_
#define UI_GFX_MOJOM_COLOR_SPACE_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/mojom/color_space.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<gfx::mojom::ColorSpacePrimaryID, gfx::ColorSpace::PrimaryID> {
  static gfx::mojom::ColorSpacePrimaryID ToMojom(
      gfx::ColorSpace::PrimaryID input) {
    switch (input) {
      case gfx::ColorSpace::PrimaryID::INVALID:
        return gfx::mojom::ColorSpacePrimaryID::INVALID;
      case gfx::ColorSpace::PrimaryID::BT709:
        return gfx::mojom::ColorSpacePrimaryID::BT709;
      case gfx::ColorSpace::PrimaryID::BT470M:
        return gfx::mojom::ColorSpacePrimaryID::BT470M;
      case gfx::ColorSpace::PrimaryID::BT470BG:
        return gfx::mojom::ColorSpacePrimaryID::BT470BG;
      case gfx::ColorSpace::PrimaryID::SMPTE170M:
        return gfx::mojom::ColorSpacePrimaryID::SMPTE170M;
      case gfx::ColorSpace::PrimaryID::SMPTE240M:
        return gfx::mojom::ColorSpacePrimaryID::SMPTE240M;
      case gfx::ColorSpace::PrimaryID::FILM:
        return gfx::mojom::ColorSpacePrimaryID::FILM;
      case gfx::ColorSpace::PrimaryID::BT2020:
        return gfx::mojom::ColorSpacePrimaryID::BT2020;
      case gfx::ColorSpace::PrimaryID::SMPTEST428_1:
        return gfx::mojom::ColorSpacePrimaryID::SMPTEST428_1;
      case gfx::ColorSpace::PrimaryID::SMPTEST431_2:
        return gfx::mojom::ColorSpacePrimaryID::SMPTEST431_2;
      case gfx::ColorSpace::PrimaryID::P3:
        return gfx::mojom::ColorSpacePrimaryID::P3;
      case gfx::ColorSpace::PrimaryID::XYZ_D50:
        return gfx::mojom::ColorSpacePrimaryID::XYZ_D50;
      case gfx::ColorSpace::PrimaryID::ADOBE_RGB:
        return gfx::mojom::ColorSpacePrimaryID::ADOBE_RGB;
      case gfx::ColorSpace::PrimaryID::APPLE_GENERIC_RGB:
        return gfx::mojom::ColorSpacePrimaryID::APPLE_GENERIC_RGB;
      case gfx::ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN:
        return gfx::mojom::ColorSpacePrimaryID::WIDE_GAMUT_COLOR_SPIN;
      case gfx::ColorSpace::PrimaryID::CUSTOM:
        return gfx::mojom::ColorSpacePrimaryID::CUSTOM;
      case gfx::ColorSpace::PrimaryID::EBU_3213_E:
        return gfx::mojom::ColorSpacePrimaryID::EBU_3213_E;
    }
    NOTREACHED_IN_MIGRATION();
    return gfx::mojom::ColorSpacePrimaryID::INVALID;
  }

  static bool FromMojom(gfx::mojom::ColorSpacePrimaryID input,
                        gfx::ColorSpace::PrimaryID* out) {
    switch (input) {
      case gfx::mojom::ColorSpacePrimaryID::INVALID:
        *out = gfx::ColorSpace::PrimaryID::INVALID;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::BT709:
        *out = gfx::ColorSpace::PrimaryID::BT709;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::BT470M:
        *out = gfx::ColorSpace::PrimaryID::BT470M;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::BT470BG:
        *out = gfx::ColorSpace::PrimaryID::BT470BG;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::SMPTE170M:
        *out = gfx::ColorSpace::PrimaryID::SMPTE170M;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::SMPTE240M:
        *out = gfx::ColorSpace::PrimaryID::SMPTE240M;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::FILM:
        *out = gfx::ColorSpace::PrimaryID::FILM;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::BT2020:
        *out = gfx::ColorSpace::PrimaryID::BT2020;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::SMPTEST428_1:
        *out = gfx::ColorSpace::PrimaryID::SMPTEST428_1;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::SMPTEST431_2:
        *out = gfx::ColorSpace::PrimaryID::SMPTEST431_2;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::P3:
        *out = gfx::ColorSpace::PrimaryID::P3;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::XYZ_D50:
        *out = gfx::ColorSpace::PrimaryID::XYZ_D50;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::ADOBE_RGB:
        *out = gfx::ColorSpace::PrimaryID::ADOBE_RGB;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::APPLE_GENERIC_RGB:
        *out = gfx::ColorSpace::PrimaryID::APPLE_GENERIC_RGB;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::WIDE_GAMUT_COLOR_SPIN:
        *out = gfx::ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::CUSTOM:
        *out = gfx::ColorSpace::PrimaryID::CUSTOM;
        return true;
      case gfx::mojom::ColorSpacePrimaryID::EBU_3213_E:
        *out = gfx::ColorSpace::PrimaryID::EBU_3213_E;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct EnumTraits<gfx::mojom::ColorSpaceTransferID,
                  gfx::ColorSpace::TransferID> {
  static gfx::mojom::ColorSpaceTransferID ToMojom(
      gfx::ColorSpace::TransferID input) {
    switch (input) {
      case gfx::ColorSpace::TransferID::INVALID:
        return gfx::mojom::ColorSpaceTransferID::INVALID;
      case gfx::ColorSpace::TransferID::BT709:
        return gfx::mojom::ColorSpaceTransferID::BT709;
      case gfx::ColorSpace::TransferID::BT709_APPLE:
        return gfx::mojom::ColorSpaceTransferID::BT709_APPLE;
      case gfx::ColorSpace::TransferID::GAMMA18:
        return gfx::mojom::ColorSpaceTransferID::GAMMA18;
      case gfx::ColorSpace::TransferID::GAMMA22:
        return gfx::mojom::ColorSpaceTransferID::GAMMA22;
      case gfx::ColorSpace::TransferID::GAMMA24:
        return gfx::mojom::ColorSpaceTransferID::GAMMA24;
      case gfx::ColorSpace::TransferID::GAMMA28:
        return gfx::mojom::ColorSpaceTransferID::GAMMA28;
      case gfx::ColorSpace::TransferID::SMPTE170M:
        return gfx::mojom::ColorSpaceTransferID::SMPTE170M;
      case gfx::ColorSpace::TransferID::SMPTE240M:
        return gfx::mojom::ColorSpaceTransferID::SMPTE240M;
      case gfx::ColorSpace::TransferID::LINEAR:
        return gfx::mojom::ColorSpaceTransferID::LINEAR;
      case gfx::ColorSpace::TransferID::LOG:
        return gfx::mojom::ColorSpaceTransferID::LOG;
      case gfx::ColorSpace::TransferID::LOG_SQRT:
        return gfx::mojom::ColorSpaceTransferID::LOG_SQRT;
      case gfx::ColorSpace::TransferID::IEC61966_2_4:
        return gfx::mojom::ColorSpaceTransferID::IEC61966_2_4;
      case gfx::ColorSpace::TransferID::BT1361_ECG:
        return gfx::mojom::ColorSpaceTransferID::BT1361_ECG;
      case gfx::ColorSpace::TransferID::SRGB:
        return gfx::mojom::ColorSpaceTransferID::SRGB;
      case gfx::ColorSpace::TransferID::BT2020_10:
        return gfx::mojom::ColorSpaceTransferID::BT2020_10;
      case gfx::ColorSpace::TransferID::BT2020_12:
        return gfx::mojom::ColorSpaceTransferID::BT2020_12;
      case gfx::ColorSpace::TransferID::PQ:
        return gfx::mojom::ColorSpaceTransferID::PQ;
      case gfx::ColorSpace::TransferID::SMPTEST428_1:
        return gfx::mojom::ColorSpaceTransferID::SMPTEST428_1;
      case gfx::ColorSpace::TransferID::HLG:
        return gfx::mojom::ColorSpaceTransferID::HLG;
      case gfx::ColorSpace::TransferID::SRGB_HDR:
        return gfx::mojom::ColorSpaceTransferID::SRGB_HDR;
      case gfx::ColorSpace::TransferID::LINEAR_HDR:
        return gfx::mojom::ColorSpaceTransferID::LINEAR_HDR;
      case gfx::ColorSpace::TransferID::CUSTOM:
        return gfx::mojom::ColorSpaceTransferID::CUSTOM;
      case gfx::ColorSpace::TransferID::CUSTOM_HDR:
        return gfx::mojom::ColorSpaceTransferID::CUSTOM_HDR;
      case gfx::ColorSpace::TransferID::PIECEWISE_HDR:
        return gfx::mojom::ColorSpaceTransferID::PIECEWISE_HDR;
      case gfx::ColorSpace::TransferID::SCRGB_LINEAR_80_NITS:
        return gfx::mojom::ColorSpaceTransferID::SCRGB_LINEAR_80_NITS;
    }
    NOTREACHED_IN_MIGRATION();
    return gfx::mojom::ColorSpaceTransferID::INVALID;
  }

  static bool FromMojom(gfx::mojom::ColorSpaceTransferID input,
                        gfx::ColorSpace::TransferID* out) {
    switch (input) {
      case gfx::mojom::ColorSpaceTransferID::INVALID:
        *out = gfx::ColorSpace::TransferID::INVALID;
        return true;
      case gfx::mojom::ColorSpaceTransferID::BT709:
        *out = gfx::ColorSpace::TransferID::BT709;
        return true;
      case gfx::mojom::ColorSpaceTransferID::BT709_APPLE:
        *out = gfx::ColorSpace::TransferID::BT709_APPLE;
        return true;
      case gfx::mojom::ColorSpaceTransferID::GAMMA18:
        *out = gfx::ColorSpace::TransferID::GAMMA18;
        return true;
      case gfx::mojom::ColorSpaceTransferID::GAMMA22:
        *out = gfx::ColorSpace::TransferID::GAMMA22;
        return true;
      case gfx::mojom::ColorSpaceTransferID::GAMMA24:
        *out = gfx::ColorSpace::TransferID::GAMMA24;
        return true;
      case gfx::mojom::ColorSpaceTransferID::GAMMA28:
        *out = gfx::ColorSpace::TransferID::GAMMA28;
        return true;
      case gfx::mojom::ColorSpaceTransferID::SMPTE170M:
        *out = gfx::ColorSpace::TransferID::SMPTE170M;
        return true;
      case gfx::mojom::ColorSpaceTransferID::SMPTE240M:
        *out = gfx::ColorSpace::TransferID::SMPTE240M;
        return true;
      case gfx::mojom::ColorSpaceTransferID::LINEAR:
        *out = gfx::ColorSpace::TransferID::LINEAR;
        return true;
      case gfx::mojom::ColorSpaceTransferID::LOG:
        *out = gfx::ColorSpace::TransferID::LOG;
        return true;
      case gfx::mojom::ColorSpaceTransferID::LOG_SQRT:
        *out = gfx::ColorSpace::TransferID::LOG_SQRT;
        return true;
      case gfx::mojom::ColorSpaceTransferID::IEC61966_2_4:
        *out = gfx::ColorSpace::TransferID::IEC61966_2_4;
        return true;
      case gfx::mojom::ColorSpaceTransferID::BT1361_ECG:
        *out = gfx::ColorSpace::TransferID::BT1361_ECG;
        return true;
      case gfx::mojom::ColorSpaceTransferID::SRGB:
        *out = gfx::ColorSpace::TransferID::SRGB;
        return true;
      case gfx::mojom::ColorSpaceTransferID::BT2020_10:
        *out = gfx::ColorSpace::TransferID::BT2020_10;
        return true;
      case gfx::mojom::ColorSpaceTransferID::BT2020_12:
        *out = gfx::ColorSpace::TransferID::BT2020_12;
        return true;
      case gfx::mojom::ColorSpaceTransferID::PQ:
        *out = gfx::ColorSpace::TransferID::PQ;
        return true;
      case gfx::mojom::ColorSpaceTransferID::SMPTEST428_1:
        *out = gfx::ColorSpace::TransferID::SMPTEST428_1;
        return true;
      case gfx::mojom::ColorSpaceTransferID::HLG:
        *out = gfx::ColorSpace::TransferID::HLG;
        return true;
      case gfx::mojom::ColorSpaceTransferID::SRGB_HDR:
        *out = gfx::ColorSpace::TransferID::SRGB_HDR;
        return true;
      case gfx::mojom::ColorSpaceTransferID::LINEAR_HDR:
        *out = gfx::ColorSpace::TransferID::LINEAR_HDR;
        return true;
      case gfx::mojom::ColorSpaceTransferID::CUSTOM:
        *out = gfx::ColorSpace::TransferID::CUSTOM;
        return true;
      case gfx::mojom::ColorSpaceTransferID::CUSTOM_HDR:
        *out = gfx::ColorSpace::TransferID::CUSTOM_HDR;
        return true;
      case gfx::mojom::ColorSpaceTransferID::PIECEWISE_HDR:
        *out = gfx::ColorSpace::TransferID::PIECEWISE_HDR;
        return true;
      case gfx::mojom::ColorSpaceTransferID::SCRGB_LINEAR_80_NITS:
        *out = gfx::ColorSpace::TransferID::SCRGB_LINEAR_80_NITS;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct EnumTraits<gfx::mojom::ColorSpaceMatrixID, gfx::ColorSpace::MatrixID> {
  static gfx::mojom::ColorSpaceMatrixID ToMojom(
      gfx::ColorSpace::MatrixID input) {
    switch (input) {
      case gfx::ColorSpace::MatrixID::INVALID:
        return gfx::mojom::ColorSpaceMatrixID::INVALID;
      case gfx::ColorSpace::MatrixID::RGB:
        return gfx::mojom::ColorSpaceMatrixID::RGB;
      case gfx::ColorSpace::MatrixID::BT709:
        return gfx::mojom::ColorSpaceMatrixID::BT709;
      case gfx::ColorSpace::MatrixID::FCC:
        return gfx::mojom::ColorSpaceMatrixID::FCC;
      case gfx::ColorSpace::MatrixID::BT470BG:
        return gfx::mojom::ColorSpaceMatrixID::BT470BG;
      case gfx::ColorSpace::MatrixID::SMPTE170M:
        return gfx::mojom::ColorSpaceMatrixID::SMPTE170M;
      case gfx::ColorSpace::MatrixID::SMPTE240M:
        return gfx::mojom::ColorSpaceMatrixID::SMPTE240M;
      case gfx::ColorSpace::MatrixID::YCOCG:
        return gfx::mojom::ColorSpaceMatrixID::YCOCG;
      case gfx::ColorSpace::MatrixID::BT2020_NCL:
        return gfx::mojom::ColorSpaceMatrixID::BT2020_NCL;
      case gfx::ColorSpace::MatrixID::YDZDX:
        return gfx::mojom::ColorSpaceMatrixID::YDZDX;
      case gfx::ColorSpace::MatrixID::GBR:
        return gfx::mojom::ColorSpaceMatrixID::GBR;
    }
    NOTREACHED_IN_MIGRATION();
    return gfx::mojom::ColorSpaceMatrixID::INVALID;
  }

  static bool FromMojom(gfx::mojom::ColorSpaceMatrixID input,
                        gfx::ColorSpace::MatrixID* out) {
    switch (input) {
      case gfx::mojom::ColorSpaceMatrixID::INVALID:
        *out = gfx::ColorSpace::MatrixID::INVALID;
        return true;
      case gfx::mojom::ColorSpaceMatrixID::RGB:
        *out = gfx::ColorSpace::MatrixID::RGB;
        return true;
      case gfx::mojom::ColorSpaceMatrixID::BT709:
        *out = gfx::ColorSpace::MatrixID::BT709;
        return true;
      case gfx::mojom::ColorSpaceMatrixID::FCC:
        *out = gfx::ColorSpace::MatrixID::FCC;
        return true;
      case gfx::mojom::ColorSpaceMatrixID::BT470BG:
        *out = gfx::ColorSpace::MatrixID::BT470BG;
        return true;
      case gfx::mojom::ColorSpaceMatrixID::SMPTE170M:
        *out = gfx::ColorSpace::MatrixID::SMPTE170M;
        return true;
      case gfx::mojom::ColorSpaceMatrixID::SMPTE240M:
        *out = gfx::ColorSpace::MatrixID::SMPTE240M;
        return true;
      case gfx::mojom::ColorSpaceMatrixID::YCOCG:
        *out = gfx::ColorSpace::MatrixID::YCOCG;
        return true;
      case gfx::mojom::ColorSpaceMatrixID::BT2020_NCL:
        *out = gfx::ColorSpace::MatrixID::BT2020_NCL;
        return true;
      case gfx::mojom::ColorSpaceMatrixID::YDZDX:
        *out = gfx::ColorSpace::MatrixID::YDZDX;
        return true;
      case gfx::mojom::ColorSpaceMatrixID::GBR:
        *out = gfx::ColorSpace::MatrixID::GBR;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct EnumTraits<gfx::mojom::ColorSpaceRangeID, gfx::ColorSpace::RangeID> {
  static gfx::mojom::ColorSpaceRangeID ToMojom(gfx::ColorSpace::RangeID input) {
    switch (input) {
      case gfx::ColorSpace::RangeID::INVALID:
        return gfx::mojom::ColorSpaceRangeID::INVALID;
      case gfx::ColorSpace::RangeID::LIMITED:
        return gfx::mojom::ColorSpaceRangeID::LIMITED;
      case gfx::ColorSpace::RangeID::FULL:
        return gfx::mojom::ColorSpaceRangeID::FULL;
      case gfx::ColorSpace::RangeID::DERIVED:
        return gfx::mojom::ColorSpaceRangeID::DERIVED;
    }
    NOTREACHED_IN_MIGRATION();
    return gfx::mojom::ColorSpaceRangeID::INVALID;
  }

  static bool FromMojom(gfx::mojom::ColorSpaceRangeID input,
                        gfx::ColorSpace::RangeID* out) {
    switch (input) {
      case gfx::mojom::ColorSpaceRangeID::INVALID:
        *out = gfx::ColorSpace::RangeID::INVALID;
        return true;
      case gfx::mojom::ColorSpaceRangeID::LIMITED:
        *out = gfx::ColorSpace::RangeID::LIMITED;
        return true;
      case gfx::mojom::ColorSpaceRangeID::FULL:
        *out = gfx::ColorSpace::RangeID::FULL;
        return true;
      case gfx::mojom::ColorSpaceRangeID::DERIVED:
        *out = gfx::ColorSpace::RangeID::DERIVED;
        return true;
    }
    NOTREACHED_IN_MIGRATION();
    return false;
  }
};

template <>
struct COMPONENT_EXPORT(GFX_SHARED_MOJOM_TRAITS)
    StructTraits<gfx::mojom::ColorSpaceDataView, gfx::ColorSpace> {
  static gfx::ColorSpace::PrimaryID primaries(const gfx::ColorSpace& input) {
    return input.primaries_;
  }

  static gfx::ColorSpace::TransferID transfer(const gfx::ColorSpace& input) {
    return input.transfer_;
  }

  static gfx::ColorSpace::MatrixID matrix(const gfx::ColorSpace& input) {
    return input.matrix_;
  }

  static gfx::ColorSpace::RangeID range(const gfx::ColorSpace& input) {
    return input.range_;
  }

  static base::span<const float> custom_primary_matrix(
      const gfx::ColorSpace& input) {
    return input.custom_primary_matrix_;
  }

  static base::span<const float> transfer_params(const gfx::ColorSpace& input) {
    return input.transfer_params_;
  }

  static bool Read(gfx::mojom::ColorSpaceDataView data, gfx::ColorSpace* out);
};

}  // namespace mojo

#endif  // UI_GFX_MOJOM_COLOR_SPACE_MOJOM_TRAITS_H_
