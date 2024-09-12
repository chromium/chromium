// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space_win.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "third_party/skia/modules/skcms/skcms.h"

namespace gfx {

DXVA2_ExtendedFormat ColorSpaceWin::GetExtendedFormat(
    const ColorSpace& color_space) {
  DXVA2_ExtendedFormat format;
  memset(&format, 0, sizeof(format));
  format.SampleFormat = DXVA2_SampleProgressiveFrame;
  format.VideoLighting = DXVA2_VideoLighting_dim;
  format.NominalRange = DXVA2_NominalRange_16_235;
  format.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
  format.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
  format.VideoTransferFunction = DXVA2_VideoTransFunc_709;

  switch (color_space.GetRangeID()) {
    case gfx::ColorSpace::RangeID::LIMITED:
      format.NominalRange = DXVA2_NominalRange_16_235;
      break;
    case gfx::ColorSpace::RangeID::FULL:
      format.NominalRange = DXVA2_NominalRange_0_255;
      break;

    case gfx::ColorSpace::RangeID::DERIVED:
    case gfx::ColorSpace::RangeID::INVALID:
      // Not handled
      break;
  }

  switch (color_space.GetMatrixID()) {
    case gfx::ColorSpace::MatrixID::BT709:
      format.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT709;
      break;
    case gfx::ColorSpace::MatrixID::BT470BG:
    case gfx::ColorSpace::MatrixID::SMPTE170M:
      format.VideoTransferMatrix = DXVA2_VideoTransferMatrix_BT601;
      break;
    case gfx::ColorSpace::MatrixID::SMPTE240M:
      format.VideoTransferMatrix = DXVA2_VideoTransferMatrix_SMPTE240M;
      break;

    case gfx::ColorSpace::MatrixID::RGB:
    case gfx::ColorSpace::MatrixID::GBR:
    case gfx::ColorSpace::MatrixID::FCC:
    case gfx::ColorSpace::MatrixID::YCOCG:
    case gfx::ColorSpace::MatrixID::BT2020_NCL:
    case gfx::ColorSpace::MatrixID::YDZDX:
    case gfx::ColorSpace::MatrixID::INVALID:
      // Not handled
      break;
  }

  switch (color_space.GetPrimaryID()) {
    case gfx::ColorSpace::PrimaryID::BT709:
      format.VideoPrimaries = DXVA2_VideoPrimaries_BT709;
      break;
    case gfx::ColorSpace::PrimaryID::BT470M:
      format.VideoPrimaries = DXVA2_VideoPrimaries_BT470_2_SysM;
      break;
    case gfx::ColorSpace::PrimaryID::BT470BG:
      format.VideoPrimaries = DXVA2_VideoPrimaries_BT470_2_SysBG;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
      format.VideoPrimaries = DXVA2_VideoPrimaries_SMPTE170M;
      break;
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
      format.VideoPrimaries = DXVA2_VideoPrimaries_SMPTE240M;
      break;
    case gfx::ColorSpace::PrimaryID::EBU_3213_E:
      format.VideoPrimaries = DXVA2_VideoPrimaries_EBU3213;
      break;

    case gfx::ColorSpace::PrimaryID::FILM:
    case gfx::ColorSpace::PrimaryID::BT2020:
    case gfx::ColorSpace::PrimaryID::SMPTEST428_1:
    case gfx::ColorSpace::PrimaryID::SMPTEST431_2:
    case gfx::ColorSpace::PrimaryID::P3:
    case gfx::ColorSpace::PrimaryID::XYZ_D50:
    case gfx::ColorSpace::PrimaryID::ADOBE_RGB:
    case gfx::ColorSpace::PrimaryID::APPLE_GENERIC_RGB:
    case gfx::ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN:
    case gfx::ColorSpace::PrimaryID::CUSTOM:
    case gfx::ColorSpace::PrimaryID::INVALID:
      // Not handled
      break;
  }

  switch (color_space.GetTransferID()) {
    case gfx::ColorSpace::TransferID::BT709:
    case gfx::ColorSpace::TransferID::SMPTE170M:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_709;
      break;
    case gfx::ColorSpace::TransferID::SMPTE240M:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_240M;
      break;
    case gfx::ColorSpace::TransferID::GAMMA22:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_22;
      break;
    case gfx::ColorSpace::TransferID::GAMMA28:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_28;
      break;
    case gfx::ColorSpace::TransferID::LINEAR:
    case gfx::ColorSpace::TransferID::LINEAR_HDR:
    case gfx::ColorSpace::TransferID::SCRGB_LINEAR_80_NITS:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_10;
      break;
    case gfx::ColorSpace::TransferID::SRGB:
    case gfx::ColorSpace::TransferID::SRGB_HDR:
      format.VideoTransferFunction = DXVA2_VideoTransFunc_sRGB;
      break;

    case gfx::ColorSpace::TransferID::LOG:
    case gfx::ColorSpace::TransferID::LOG_SQRT:
    case gfx::ColorSpace::TransferID::IEC61966_2_4:
    case gfx::ColorSpace::TransferID::BT1361_ECG:
    case gfx::ColorSpace::TransferID::BT2020_10:
    case gfx::ColorSpace::TransferID::BT2020_12:
    case gfx::ColorSpace::TransferID::PQ:
    case gfx::ColorSpace::TransferID::SMPTEST428_1:
    case gfx::ColorSpace::TransferID::HLG:
    case gfx::ColorSpace::TransferID::BT709_APPLE:
    case gfx::ColorSpace::TransferID::GAMMA18:
    case gfx::ColorSpace::TransferID::GAMMA24:
    case gfx::ColorSpace::TransferID::CUSTOM:
    case gfx::ColorSpace::TransferID::CUSTOM_HDR:
    case gfx::ColorSpace::TransferID::PIECEWISE_HDR:
    case gfx::ColorSpace::TransferID::INVALID:
      // Not handled
      break;
  }

  return format;
}

bool ColorSpaceWin::CanConvertToDXGIColorSpace(const ColorSpace& color_space) {
  // RGB color space is not supported yet.
  DCHECK_NE(color_space.GetMatrixID(), gfx::ColorSpace::MatrixID::RGB);
  switch (color_space.GetRangeID()) {
    case gfx::ColorSpace::RangeID::LIMITED:
    case gfx::ColorSpace::RangeID::FULL:
      break;

    case gfx::ColorSpace::RangeID::DERIVED:
    case gfx::ColorSpace::RangeID::INVALID:
      // Assuming limited.
      break;
  }

  switch (color_space.GetMatrixID()) {
    case gfx::ColorSpace::MatrixID::BT709:
    case gfx::ColorSpace::MatrixID::BT470BG:
    case gfx::ColorSpace::MatrixID::SMPTE170M:
    case gfx::ColorSpace::MatrixID::SMPTE240M:
    case gfx::ColorSpace::MatrixID::BT2020_NCL:
      break;

    case gfx::ColorSpace::MatrixID::INVALID:
      // Assuming BT709.
      break;

    case gfx::ColorSpace::MatrixID::RGB:
    case gfx::ColorSpace::MatrixID::GBR:
    case gfx::ColorSpace::MatrixID::FCC:
    case gfx::ColorSpace::MatrixID::YCOCG:
    case gfx::ColorSpace::MatrixID::YDZDX:
      // Not supported.
      return false;
  }

  switch (color_space.GetPrimaryID()) {
    case gfx::ColorSpace::PrimaryID::BT709:
    case gfx::ColorSpace::PrimaryID::BT470BG:
    case gfx::ColorSpace::PrimaryID::SMPTE170M:
    case gfx::ColorSpace::PrimaryID::SMPTE240M:
    case gfx::ColorSpace::PrimaryID::BT2020:
      break;

    case gfx::ColorSpace::PrimaryID::INVALID:
      // Assuming BT709.
      break;

    case gfx::ColorSpace::PrimaryID::BT470M:
    case gfx::ColorSpace::PrimaryID::FILM:
    case gfx::ColorSpace::PrimaryID::SMPTEST428_1:
    case gfx::ColorSpace::PrimaryID::SMPTEST431_2:
    case gfx::ColorSpace::PrimaryID::P3:
    case gfx::ColorSpace::PrimaryID::XYZ_D50:
    case gfx::ColorSpace::PrimaryID::ADOBE_RGB:
    case gfx::ColorSpace::PrimaryID::APPLE_GENERIC_RGB:
    case gfx::ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN:
    case gfx::ColorSpace::PrimaryID::CUSTOM:
    case gfx::ColorSpace::PrimaryID::EBU_3213_E:
      // Not supported.
      return false;
  }

  switch (color_space.GetTransferID()) {
    case gfx::ColorSpace::TransferID::BT709:
    case gfx::ColorSpace::TransferID::GAMMA28:
    case gfx::ColorSpace::TransferID::SMPTE170M:
    case gfx::ColorSpace::TransferID::SMPTE240M:
    case gfx::ColorSpace::TransferID::SRGB:
    case gfx::ColorSpace::TransferID::BT2020_10:
    case gfx::ColorSpace::TransferID::BT2020_12:
    case gfx::ColorSpace::TransferID::PQ:
    case gfx::ColorSpace::TransferID::HLG:
      break;

    case gfx::ColorSpace::TransferID::INVALID:
      // Assuming BT709.
      break;

    case gfx::ColorSpace::TransferID::BT709_APPLE:
    case gfx::ColorSpace::TransferID::GAMMA18:
    case gfx::ColorSpace::TransferID::GAMMA22:
    case gfx::ColorSpace::TransferID::GAMMA24:
    case gfx::ColorSpace::TransferID::LINEAR:
    case gfx::ColorSpace::TransferID::LOG:
    case gfx::ColorSpace::TransferID::LOG_SQRT:
    case gfx::ColorSpace::TransferID::IEC61966_2_4:
    case gfx::ColorSpace::TransferID::BT1361_ECG:
    case gfx::ColorSpace::TransferID::SMPTEST428_1:
    case gfx::ColorSpace::TransferID::SRGB_HDR:
    case gfx::ColorSpace::TransferID::LINEAR_HDR:
    case gfx::ColorSpace::TransferID::CUSTOM:
    case gfx::ColorSpace::TransferID::CUSTOM_HDR:
    case gfx::ColorSpace::TransferID::PIECEWISE_HDR:
    case gfx::ColorSpace::TransferID::SCRGB_LINEAR_80_NITS:
      // Not supported.
      return false;
  }

  return true;
}

DXGI_COLOR_SPACE_TYPE ColorSpaceWin::GetDXGIColorSpace(
    const ColorSpace& color_space,
    bool force_yuv) {
  // Treat invalid color space as sRGB.
  if (!color_space.IsValid()) {
    return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
  }

  if (color_space.GetMatrixID() == gfx::ColorSpace::MatrixID::RGB &&
      !force_yuv) {
    // For RGB, we default to FULL
    if (color_space.GetRangeID() == gfx::ColorSpace::RangeID::LIMITED) {
      if (color_space.GetPrimaryID() == gfx::ColorSpace::PrimaryID::BT2020) {
        switch (color_space.GetTransferID()) {
          case gfx::ColorSpace::TransferID::PQ:
            return DXGI_COLOR_SPACE_RGB_STUDIO_G2084_NONE_P2020;

          default:
            return DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P2020;
        }
      } else {
        return DXGI_COLOR_SPACE_RGB_STUDIO_G22_NONE_P709;
      }
    } else {
      if (color_space.GetPrimaryID() == gfx::ColorSpace::PrimaryID::BT2020) {
        switch (color_space.GetTransferID()) {
          case gfx::ColorSpace::TransferID::PQ:
            return DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;

          default:
            return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020;
        }
      } else {
        switch (color_space.GetTransferID()) {
          case gfx::ColorSpace::TransferID::LINEAR:
          case gfx::ColorSpace::TransferID::LINEAR_HDR:
          case gfx::ColorSpace::TransferID::SCRGB_LINEAR_80_NITS:
            return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;

          case gfx::ColorSpace::TransferID::CUSTOM_HDR: {
            skcms_TransferFunction fn;
            color_space.GetTransferFunction(&fn);
            if (fn.g == 1.f) {
              return DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
            } else {
              DLOG(ERROR) << "Windows HDR only supports gamma=1.0.";
              return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
            }
          }

          default:
            return DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        }
      }
    }
  } else {
    if (color_space.GetPrimaryID() == gfx::ColorSpace::PrimaryID::BT2020) {
      // For YUV, we default to LIMITED
      if (color_space.GetRangeID() == gfx::ColorSpace::RangeID::FULL) {
        switch (color_space.GetTransferID()) {
          case gfx::ColorSpace::TransferID::PQ:
            // Could also be DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020
            return DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020;

          case gfx::ColorSpace::TransferID::HLG:
            return DXGI_COLOR_SPACE_YCBCR_FULL_GHLG_TOPLEFT_P2020;

          default:
            return DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020;
        }
      }
      // RangeID::LIMITED handling.
      switch (color_space.GetTransferID()) {
        case gfx::ColorSpace::TransferID::PQ:
          // Could also be DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_LEFT_P2020
          return DXGI_COLOR_SPACE_YCBCR_STUDIO_G2084_TOPLEFT_P2020;

        case gfx::ColorSpace::TransferID::HLG:
          // Note: This may not always work. See https://crbug.com/1144260#c6.
          return DXGI_COLOR_SPACE_YCBCR_STUDIO_GHLG_TOPLEFT_P2020;

        default:
          // Could also be DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020.
          return DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_TOPLEFT_P2020;
      }
    } else if (color_space.GetPrimaryID() ==
                   gfx::ColorSpace::PrimaryID::BT470BG ||
               color_space.GetPrimaryID() ==
                   gfx::ColorSpace::PrimaryID::SMPTE170M) {
      // For YUV, we default to LIMITED
      switch (color_space.GetRangeID()) {
        case gfx::ColorSpace::RangeID::FULL:
          return DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601;

        default:
          return DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601;
      }
    } else {
      // For YUV, we default to LIMITED
      if (color_space.GetRangeID() == gfx::ColorSpace::RangeID::FULL) {
        switch (color_space.GetTransferID()) {
          // TODO(hubbe): Check if this is correct.
          case gfx::ColorSpace::TransferID::SMPTE170M:
            return DXGI_COLOR_SPACE_YCBCR_FULL_G22_NONE_P709_X601;

          default:
            return DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709;
        }
      }
      return DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;
    }
  }
}

DXGI_FORMAT ColorSpaceWin::GetDXGIFormat(const gfx::ColorSpace& color_space) {
  // The PQ transfer function needs 10 bits.
  if (color_space.GetTransferID() == gfx::ColorSpace::TransferID::PQ)
    return DXGI_FORMAT_R10G10B10A2_UNORM;

  // Non-PQ HDR color spaces use half-float.
  if (color_space.IsHDR())
    return DXGI_FORMAT_R16G16B16A16_FLOAT;

  // For now just give everything else 8 bits. We will want to use 10 or 16 bits
  // for BT2020 gamuts.
  return DXGI_FORMAT_B8G8R8A8_UNORM;
}

D3D11_VIDEO_PROCESSOR_COLOR_SPACE ColorSpaceWin::GetD3D11ColorSpace(
    const ColorSpace& color_space) {
  D3D11_VIDEO_PROCESSOR_COLOR_SPACE ret = {0};
  if (color_space.GetRangeID() == gfx::ColorSpace::RangeID::FULL) {
    ret.RGB_Range = 0;  // FULL
    ret.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_0_255;
  } else {
    ret.RGB_Range = 1;  // LIMITED
    ret.Nominal_Range = D3D11_VIDEO_PROCESSOR_NOMINAL_RANGE_16_235;
  }

  switch (color_space.GetMatrixID()) {
    case gfx::ColorSpace::MatrixID::BT709:
      ret.YCbCr_Matrix = 1;
      break;

    case gfx::ColorSpace::MatrixID::BT470BG:
    case gfx::ColorSpace::MatrixID::SMPTE170M:
      ret.YCbCr_Matrix = 0;
      break;

    case gfx::ColorSpace::MatrixID::SMPTE240M:
    case gfx::ColorSpace::MatrixID::RGB:
    case gfx::ColorSpace::MatrixID::GBR:
    case gfx::ColorSpace::MatrixID::FCC:
    case gfx::ColorSpace::MatrixID::YCOCG:
    case gfx::ColorSpace::MatrixID::BT2020_NCL:
    case gfx::ColorSpace::MatrixID::YDZDX:
    case gfx::ColorSpace::MatrixID::INVALID:
      // Not handled
      break;
  }
  return ret;
}

}  // namespace gfx
