// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_H_
#define UI_GFX_COLOR_SPACE_H_

#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/color_space_export.h"

namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace gfx {

class ICCProfile;

// Used to represet a color space for the purpose of color conversion.
// This is designed to be safe and compact enough to send over IPC
// between any processes.
class COLOR_SPACE_EXPORT ColorSpace {
 public:
  enum class PrimaryID : uint8_t {
    INVALID,
    BT709,
    BT470M,
    BT470BG,
    SMPTE170M,
    SMPTE240M,
    FILM,
    BT2020,
    SMPTEST428_1,
    SMPTEST431_2,
    SMPTEST432_1,
    XYZ_D50,
    ADOBE_RGB,
    // Corresponds the the primaries of the "Generic RGB" profile used in the
    // Apple ColorSync application, used by layout tests on Mac.
    APPLE_GENERIC_RGB,
    // A very wide gamut space with rotated primaries. Used by layout tests.
    WIDE_GAMUT_COLOR_SPIN,
    // Primaries defined by the primary matrix |custom_primary_matrix_|.
    CUSTOM,
    LAST = CUSTOM,
  };

  enum class TransferID : uint8_t {
    INVALID,
    BT709,
    // On macOS, BT709 hardware decoded video frames, when displayed as
    // overlays, will have a transfer function of gamma=1.961.
    BT709_APPLE,
    GAMMA18,
    GAMMA22,
    GAMMA24,
    GAMMA28,
    SMPTE170M,
    SMPTE240M,
    LINEAR,
    LOG,
    LOG_SQRT,
    IEC61966_2_4,
    BT1361_ECG,
    IEC61966_2_1,
    BT2020_10,
    BT2020_12,
    SMPTEST2084,
    SMPTEST428_1,
    ARIB_STD_B67,  // AKA hybrid-log gamma, HLG.
    // This is an ad-hoc transfer function that decodes SMPTE 2084 content
    // into a [0, 1] range more or less suitable for viewing on a non-hdr
    // display.
    SMPTEST2084_NON_HDR,
    // The same as IEC61966_2_1 on the interval [0, 1], with the nonlinear
    // segment continuing beyond 1 and point symmetry defining values below 0.
    IEC61966_2_1_HDR,
    // The same as LINEAR but is defined for all real values.
    LINEAR_HDR,
    // A parametric transfer function defined by |custom_transfer_params_|.
    CUSTOM,
    LAST = CUSTOM,
  };

  enum class MatrixID : uint8_t {
    INVALID,
    RGB,
    BT709,
    FCC,
    BT470BG,
    SMPTE170M,
    SMPTE240M,
    YCOCG,
    BT2020_NCL,
    BT2020_CL,
    YDZDX,
    LAST = YDZDX,
  };

  enum class RangeID : uint8_t {
    INVALID,
    // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
    LIMITED,
    // Full RGB color range with RGB valees from 0 to 255.
    FULL,
    // Range is defined by TransferID/MatrixID.
    DERIVED,
    LAST = DERIVED,
  };

  constexpr ColorSpace() {}
  ColorSpace(PrimaryID primaries, TransferID transfer);
  constexpr ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID full_range)
      : primaries_(primaries),
        transfer_(transfer),
        matrix_(matrix),
        range_(full_range) {}

  ColorSpace(PrimaryID primaries,
             const SkColorSpaceTransferFn& fn,
             MatrixID matrix,
             RangeID full_range);
  explicit ColorSpace(const SkColorSpace& sk_color_space);

  // Returns true if this is not the default-constructor object.
  bool IsValid() const;

  static constexpr ColorSpace CreateSRGB() {
    return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1, MatrixID::RGB,
                      RangeID::FULL);
  }

  static constexpr ColorSpace CreateDisplayP3D65() {
    return ColorSpace(PrimaryID::SMPTEST432_1, TransferID::IEC61966_2_1,
                      MatrixID::RGB, RangeID::FULL);
  }
  static ColorSpace CreateCustom(const SkMatrix44& to_XYZD50,
                                 TransferID transfer_id);
  static ColorSpace CreateCustom(const SkMatrix44& to_XYZD50,
                                 const SkColorSpaceTransferFn& fn);
  static constexpr ColorSpace CreateXYZD50() {
    return ColorSpace(PrimaryID::XYZ_D50, TransferID::LINEAR, MatrixID::RGB,
                      RangeID::FULL);
  }

  // Extended sRGB matches sRGB for values in [0, 1], and extends the transfer
  // function to all real values.
  static constexpr ColorSpace CreateExtendedSRGB() {
    return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1_HDR,
                      MatrixID::RGB, RangeID::FULL);
  }

  // scRGB uses the same primaries as sRGB but has a linear transfer function
  // for all real values.
  static constexpr ColorSpace CreateSCRGBLinear() {
    return ColorSpace(PrimaryID::BT709, TransferID::LINEAR_HDR, MatrixID::RGB,
                      RangeID::FULL);
  }

  // TODO(ccameron): Remove these, and replace with more generic constructors.
  static constexpr ColorSpace CreateJpeg() {
    // TODO(ccameron): Determine which primaries and transfer function were
    // intended here.
    return ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1,
                      MatrixID::SMPTE170M, RangeID::FULL);
  }
  static constexpr ColorSpace CreateREC601() {
    return ColorSpace(PrimaryID::SMPTE170M, TransferID::SMPTE170M,
                      MatrixID::SMPTE170M, RangeID::LIMITED);
  }
  static constexpr ColorSpace CreateREC709() {
    return ColorSpace(PrimaryID::BT709, TransferID::BT709, MatrixID::BT709,
                      RangeID::LIMITED);
  }

  // Generates a process global unique ID that can be used to key a color space.
  static int GetNextId();
  static int kInvalidId;

  bool operator==(const ColorSpace& other) const;
  bool operator!=(const ColorSpace& other) const;
  bool operator<(const ColorSpace& other) const;
  size_t GetHash() const;
  std::string ToString() const;

  // Returns true if the decoded values can be outside of the 0.0-1.0 range.
  bool IsHDR() const;

  // Returns true if the encoded values can be outside of the 0.0-1.0 range.
  bool FullRangeEncodedValues() const;

  // Returns true if this color space is parametric (or a sufficiently accurate
  // approximation of its ICCProfile that we can use it directly).
  bool IsParametricAccurate() const;

  // Return a parametric approximation of this color space (if it is not already
  // parametric).
  ColorSpace GetParametricApproximation() const;

  // Return this color space with any YUV to RGB conversion stripped off.
  ColorSpace GetAsRGB() const;

  // Return this color space with any range adjust or YUV to RGB conversion
  // stripped off.
  ColorSpace GetAsFullRangeRGB() const;

  // Return a color space where all values are bigger/smaller by the given
  // factor. If you convert colors from SRGB to SRGB.GetScaledColorSpace(2.0)
  // everything will be half as bright in linear lumens.
  ColorSpace GetScaledColorSpace(float factor) const;

  // If |this| is the final output color space, return the color space that
  // would be appropriate for rasterization.
  ColorSpace GetRasterColorSpace() const;

  // If |this| is the final output color space, return the color space that
  // would be appropriate for blending.
  ColorSpace GetBlendingColorSpace() const;

  // This will return nullptr for non-RGB spaces, spaces with non-FULL
  // range, and unspecified spaces.
  sk_sp<SkColorSpace> ToSkColorSpace() const;

  // For YUV color spaces, return the closest SkYUVColorSpace.
  // Returns true if a close match is found.
  bool ToSkYUVColorSpace(SkYUVColorSpace* out) const;

  void GetPrimaryMatrix(SkMatrix44* to_XYZD50) const;
  bool GetTransferFunction(SkColorSpaceTransferFn* fn) const;
  bool GetInverseTransferFunction(SkColorSpaceTransferFn* fn) const;

  // For most formats, this is the RGB to YUV matrix.
  void GetTransferMatrix(SkMatrix44* matrix) const;
  void GetRangeAdjustMatrix(SkMatrix44* matrix) const;

 private:
  void SetCustomTransferFunction(const SkColorSpaceTransferFn& fn);
  void SetCustomPrimaries(const SkMatrix44& to_XYZD50);

  // Returns true if the transfer function is defined by an
  // SkColorSpaceTransferFn which is extended to all real values.
  bool HasExtendedSkTransferFn() const;

  PrimaryID primaries_ = PrimaryID::INVALID;
  TransferID transfer_ = TransferID::INVALID;
  MatrixID matrix_ = MatrixID::INVALID;
  RangeID range_ = RangeID::INVALID;

  // Only used if primaries_ is PrimaryID::CUSTOM.
  float custom_primary_matrix_[9] = {0, 0, 0, 0, 0, 0, 0, 0};

  // Only used if transfer_ is TransferID::CUSTOM. This array consists of the A
  // through G entries of the SkColorSpaceTransferFn structure in alphabetical
  // order.
  float custom_transfer_params_[7] = {0, 0, 0, 0, 0, 0, 0};

  // This is set if and only if this color space is to represent an ICC profile
  // that cannot be sufficiently accurately represented with a custom primary
  // matrix and transfer function. It can be used to look up the original
  // ICCProfile to create a LUT based transform.
  uint64_t icc_profile_id_ = 0;

  friend class ICCProfile;
  friend class ICCProfileCache;
  friend class ColorTransform;
  friend class ColorTransformInternal;
  friend class ColorSpaceWin;
  friend struct IPC::ParamTraits<ColorSpace>;
  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, GetColorSpace);
};

// Stream operator so ColorSpace can be used in assertion statements.
COLOR_SPACE_EXPORT std::ostream& operator<<(std::ostream& out,
                                            const ColorSpace& color_space);

}  // namespace gfx

#endif  // UI_GFX_COLOR_SPACE_H_
