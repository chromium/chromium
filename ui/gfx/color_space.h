// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_SPACE_H_
#define UI_GFX_COLOR_SPACE_H_

#include <stdint.h>

#include <iosfwd>
#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "build/build_config.h"
#include "skia/ext/skcolorspace_trfn.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space_export.h"

struct skcms_Matrix3x3;
struct skcms_TransferFunction;
class SkColorSpace;
class SkM44;
struct SkColorSpacePrimaries;
enum SkYUVColorSpace : int;

// These forward declarations are used to give IPC code friend access to private
// fields of gfx::ColorSpace for the purpose of serialization and
// deserialization.
namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace mojo {
template <class T, class U>
struct StructTraits;
}  // namespace mojo

// Used to serialize a gfx::ColorSpace through the GPU command buffer.
struct _GLcolorSpace;

namespace media {
namespace stable {
namespace mojom {
class ColorSpaceDataView;
}  // namespace mojom
}  // namespace stable
}  // namespace media

namespace gfx {

enum class ContentColorUsage : uint8_t;

namespace mojom {
class ColorSpaceDataView;
}  // namespace mojom

// Used to represent a color space for the purpose of color conversion.
// This is designed to be safe and compact enough to send over IPC
// between any processes.
class COLOR_SPACE_EXPORT ColorSpace {
 public:
  enum class PrimaryID : uint8_t {
    // Used as an enum for metrics. DO NOT reorder or delete values. Rather,
    // add them at the end and increment kMaxValue.
    INVALID,
    // BT709 is also the primaries for SRGB.
    BT709,
    BT470M,
    BT470BG,
    SMPTE170M,
    SMPTE240M,
    FILM,
    BT2020,
    SMPTEST428_1,
    SMPTEST431_2,
    P3,
    XYZ_D50,
    ADOBE_RGB,
    // Corresponds the the primaries of the "Generic RGB" profile used in the
    // Apple ColorSync application, used by layout tests on Mac.
    APPLE_GENERIC_RGB,
    // A very wide gamut space with rotated primaries. Used by layout tests.
    WIDE_GAMUT_COLOR_SPIN,
    // Primaries defined by the primary matrix |custom_primary_matrix_|.
    CUSTOM,
    EBU_3213_E,
    kMaxValue = EBU_3213_E,
  };

  enum class TransferID : uint8_t {
    // Used as an enum for metrics. DO NOT reorder or delete values. Rather,
    // add them at the end and increment kMaxValue.
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
    SRGB,
    BT2020_10,
    BT2020_12,
    // Perceptual quantizer, also known as SMPTEST2084.
    PQ,
    SMPTEST428_1,
    // Hybrid-log gamma, also known as ARIB_STD_B67.
    HLG,
    // The same as SRGB on the interval [0, 1], with the nonlinear segment
    // continuing beyond 1 and point symmetry defining values below 0.
    SRGB_HDR,
    // The same as LINEAR but is defined for all real values.
    LINEAR_HDR,
    // A parametric transfer function defined by |transfer_params_|.
    CUSTOM,
    // An HDR parametric transfer function defined by |transfer_params_|.
    CUSTOM_HDR,
    // An HDR transfer function that is piecewise sRGB, and piecewise linear.
    PIECEWISE_HDR,
    // An HDR transfer function that is linear, with the value 1 at 80 nits.
    // This transfer function is not SDR-referred, and therefore can only be
    // used (e.g, by ToSkColorSpace or GetTransferFunction) when an SDR white
    // level is specified.
    SCRGB_LINEAR_80_NITS,
    kMaxValue = SCRGB_LINEAR_80_NITS,
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
    YDZDX,
    GBR,
    kMaxValue = GBR,
  };

  enum class RangeID : uint8_t {
    INVALID,
    // Limited Rec. 709 color range with RGB values ranging from 16 to 235.
    LIMITED,
    // Full RGB color range with RGB values from 0 to 255.
    FULL,
    // Range is defined by TransferID/MatrixID.
    DERIVED,
    kMaxValue = DERIVED,
  };

  constexpr ColorSpace() {}
  constexpr ColorSpace(PrimaryID primaries, TransferID transfer)
      : ColorSpace(primaries, transfer, MatrixID::RGB, RangeID::FULL) {}
  constexpr ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range)
      : primaries_(primaries),
        transfer_(transfer),
        matrix_(matrix),
        range_(range) {}
  ColorSpace(PrimaryID primaries,
             TransferID transfer,
             MatrixID matrix,
             RangeID range,
             const skcms_Matrix3x3* custom_primary_matrix,
             const skcms_TransferFunction* cunstom_transfer_fn);

  explicit ColorSpace(const SkColorSpace& sk_color_space, bool is_hdr = false);

  // Returns true if this is not the default-constructor object.
  bool IsValid() const;

  static constexpr ColorSpace CreateSRGB() {
    return ColorSpace(PrimaryID::BT709, TransferID::SRGB, MatrixID::RGB,
                      RangeID::FULL);
  }

  static constexpr ColorSpace CreateDisplayP3D65() {
    return ColorSpace(PrimaryID::P3, TransferID::SRGB, MatrixID::RGB,
                      RangeID::FULL);
  }
  static ColorSpace CreateCustom(const skcms_Matrix3x3& to_XYZD50,
                                 const skcms_TransferFunction& fn);
  static ColorSpace CreateCustom(const skcms_Matrix3x3& to_XYZD50,
                                 TransferID transfer);
  static constexpr ColorSpace CreateXYZD50() {
    return ColorSpace(PrimaryID::XYZ_D50, TransferID::LINEAR, MatrixID::RGB,
                      RangeID::FULL);
  }

  // Extended sRGB matches sRGB for values in [0, 1], and extends the transfer
  // function to all real values.
  static constexpr ColorSpace CreateExtendedSRGB() {
    return ColorSpace(PrimaryID::BT709, TransferID::SRGB_HDR, MatrixID::RGB,
                      RangeID::FULL);
  }

  // scRGB uses the same primaries as sRGB but has a linear transfer function
  // for all real values.
  static constexpr ColorSpace CreateSRGBLinear() {
    return ColorSpace(PrimaryID::BT709, TransferID::LINEAR_HDR, MatrixID::RGB,
                      RangeID::FULL);
  }

  // scRGB uses the same primaries as sRGB but has a linear transfer function
  // for all real values, and an SDR white level of 80 nits.
  static constexpr ColorSpace CreateSCRGBLinear80Nits() {
    return ColorSpace(PrimaryID::BT709, TransferID::SCRGB_LINEAR_80_NITS,
                      MatrixID::RGB, RangeID::FULL);
  }

  // HDR10 uses BT.2020 primaries with SMPTE ST 2084 PQ transfer function.
  static constexpr ColorSpace CreateHDR10() {
    return ColorSpace(PrimaryID::BT2020, TransferID::PQ, MatrixID::RGB,
                      RangeID::FULL);
  }

  // HLG uses the BT.2020 primaries with the ARIB_STD_B67 transfer function.
  static constexpr ColorSpace CreateHLG() {
    return ColorSpace(PrimaryID::BT2020, TransferID::HLG, MatrixID::RGB,
                      RangeID::FULL);
  }

  // An extended sRGB ColorSpace that matches the sRGB EOTF but extends to
  // 4.99x the headroom of SDR brightness. Designed for a 10 bpc buffer format.
  // Uses P3 primaries. An HDR ColorSpace suitable for blending and compositing.
  static ColorSpace CreateExtendedSRGB10Bit();

  // Create a piecewise-HDR color space.
  // - If |primaries| is CUSTOM, then |custom_primary_matrix| must be
  //   non-nullptr.
  // - The SDR joint is the encoded pixel value where the SDR portion reaches 1,
  //   usually 0.25 or 0.5, corresponding to giving 8 or 9 of 10 bits to SDR.
  //   This must be in the open interval (0, 1).
  // - The HDR level the value that the transfer function will evaluate to at 1,
  //   and represents the maximum HDR brightness relative to the maximum SDR
  //   brightness. This must be strictly greater than 1.
  static ColorSpace CreatePiecewiseHDR(
      PrimaryID primaries,
      float sdr_joint,
      float hdr_level,
      const skcms_Matrix3x3* custom_primary_matrix = nullptr);

  // TODO(ccameron): Remove these, and replace with more generic constructors.
  static constexpr ColorSpace CreateJpeg() {
    // TODO(ccameron): Determine which primaries and transfer function were
    // intended here.
    return ColorSpace(PrimaryID::BT709, TransferID::SRGB, MatrixID::SMPTE170M,
                      RangeID::FULL);
  }
  static constexpr ColorSpace CreateREC601() {
    return ColorSpace(PrimaryID::SMPTE170M, TransferID::SMPTE170M,
                      MatrixID::SMPTE170M, RangeID::LIMITED);
  }
  static constexpr ColorSpace CreateREC709() {
    return ColorSpace(PrimaryID::BT709, TransferID::BT709, MatrixID::BT709,
                      RangeID::LIMITED);
  }

  // The default number of nits for SDR white. This is used for transformations
  // between color spaces that do not specify an SDR white for tone mapping
  // (e.g, in 2D canvas).
  static constexpr float kDefaultSDRWhiteLevel = 203.f;

  bool operator==(const ColorSpace& other) const;
  bool operator!=(const ColorSpace& other) const;
  bool operator<(const ColorSpace& other) const;
  size_t GetHash() const;
  std::string ToString() const;

  bool IsWide() const;

  // Returns true if the transfer function is an HDR one (SMPTE 2084, HLG, etc).
  bool IsHDR() const;

  // Returns true if there exists a default tone mapping that should be applied
  // when drawing content with this color space. This is true for spaces with
  // the PQ and HLG transfer functions.
  bool IsToneMappedByDefault() const;

  // Returns true if the color space's interpretation is affected by the SDR
  // white level parameter. This is true for spaces with the PQ, HLG, and
  // SCRGB_LINEAR_80_NITS transfer functions.
  bool IsAffectedBySDRWhiteLevel() const;

  // If this color space is affected by the SDR white level, return |this| with
  // its SDR white level set to |sdr_white_level|. Otherwise return |this|
  // unmodified.
  ColorSpace GetWithSdrWhiteLevel(float sdr_white_level) const;

  // Returns true if the encoded values can be outside of the 0.0-1.0 range.
  bool FullRangeEncodedValues() const;

  // Returns the color space's content color usage category (sRGB, WCG, or HDR).
  ContentColorUsage GetContentColorUsage() const;

  // Return this color space with any YUV to RGB conversion stripped off.
  ColorSpace GetAsRGB() const;

  // Return this color space with any range adjust or YUV to RGB conversion
  // stripped off.
  ColorSpace GetAsFullRangeRGB() const;

  // Return a color space where all values are bigger/smaller by the given
  // factor. If you convert colors from SRGB to SRGB.GetScaledColorSpace(2.0)
  // everything will be half as bright in linear lumens.
  ColorSpace GetScaledColorSpace(float factor) const;

  // Return true if blending in |this| is close enough to blending in sRGB to
  // be considered acceptable (only PQ and nearly-linear transfer functions
  // return false).
  bool IsSuitableForBlending() const;

  // Return a combined color space with has the same primary and transfer than
  // the caller but replacing the matrix and range with the given values.
  ColorSpace GetWithMatrixAndRange(MatrixID matrix, RangeID range) const;

  // This will return nullptr for non-RGB spaces, spaces with non-FULL
  // range, unspecified spaces, and spaces that require but are not provided
  // and SDR white level.
  sk_sp<SkColorSpace> ToSkColorSpace(
      std::optional<float> sdr_white_level = std::nullopt) const;

  // Return a GLcolorSpace value that is valid for the lifetime of |this|. This
  // function is used to serialize ColorSpace objects across the GPU command
  // buffer.
  const _GLcolorSpace* AsGLColorSpace() const;

  // For YUV color spaces, return the closest SkYUVColorSpace. Returns true if a
  // close match is found. Otherwise, leaves *out unchanged and returns false.
  // If |matrix_id| is MatrixID::BT2020_NCL and |bit_depth| is provided, a bit
  // depth appropriate SkYUVColorSpace will be provided.
  bool ToSkYUVColorSpace(int bit_depth, SkYUVColorSpace* out) const;
  bool ToSkYUVColorSpace(SkYUVColorSpace* out) const {
    return ToSkYUVColorSpace(kDefaultBitDepth, out);
  }

  // Return the RGB and whitepoint coordinates of the ColorSpace's
  // chromaticity. Assumes D65 whitepoint in the case of a custom PrimaryID.
  SkColorSpacePrimaries GetPrimaries() const;
  void GetPrimaryMatrix(skcms_Matrix3x3* to_XYZD50) const;
  SkM44 GetPrimaryMatrix() const;

  // Retrieve the parametric transfer function for this color space. Returns
  // false if none is available, or if `sdr_white_level` is required but
  // not specified.
  bool GetTransferFunction(
      skcms_TransferFunction* fn,
      std::optional<float> sdr_white_level = std::nullopt) const;
  bool GetInverseTransferFunction(
      skcms_TransferFunction* fn,
      std::optional<float> sdr_white_level = std::nullopt) const;

  // Returns the parameters for a PIECEWISE_HDR transfer function. See
  // CreatePiecewiseHDR for parameter meanings.
  bool GetPiecewiseHDRParams(float* sdr_point, float* hdr_level) const;

  // Returns the transfer matrix for |bit_depth|. For most formats, this is the
  // RGB to YUV matrix.
  SkM44 GetTransferMatrix(int bit_depth) const;

  // Returns the range adjust matrix that converts from |range_| to full range
  // for |bit_depth|.
  SkM44 GetRangeAdjustMatrix(int bit_depth) const;

  // Returns the current primary ID.
  // Note: if SetCustomPrimaries() has been used, the primary ID returned
  // may have been set to PrimaryID::CUSTOM, or been coerced to another
  // PrimaryID if it was very close.
  PrimaryID GetPrimaryID() const;

  // Returns the current transfer ID.
  TransferID GetTransferID() const;

  // Returns the current matrix ID.
  MatrixID GetMatrixID() const;

  // Returns the current range ID.
  RangeID GetRangeID() const;

  // Returns true if the transfer function is defined by an
  // skcms_TransferFunction which is extended to all real values. This is true
  // unless the color space has a non-RGB matrix.
  bool HasExtendedSkTransferFn() const;

  // Returns true if the transfer function values of this color space match
  // those of the passed in skcms_TransferFunction.
  bool IsTransferFunctionEqualTo(const skcms_TransferFunction& fn) const;

  // Returns true if each color in |other| can be expressed in this color space.
  bool Contains(const ColorSpace& other) const;

 private:
  // The default bit depth assumed by ToSkYUVColorSpace().
  static constexpr int kDefaultBitDepth = 8;

  static SkColorSpacePrimaries GetColorSpacePrimaries(
      PrimaryID,
      const skcms_Matrix3x3* custom_primary_matrix);
  static void GetPrimaryMatrix(PrimaryID, skcms_Matrix3x3* to_XYZD50);
  static bool GetTransferFunction(TransferID, skcms_TransferFunction* fn);
  static size_t TransferParamCount(TransferID);

  void SetCustomTransferFunction(const skcms_TransferFunction& fn);
  void SetCustomPrimaries(const skcms_Matrix3x3& to_XYZD50);

  PrimaryID primaries_ = PrimaryID::INVALID;
  TransferID transfer_ = TransferID::INVALID;
  MatrixID matrix_ = MatrixID::INVALID;
  RangeID range_ = RangeID::INVALID;

  // Only used if primaries_ is PrimaryID::CUSTOM.
  float custom_primary_matrix_[9] = {0};

  // Parameters for the transfer function. The interpretation depends on
  // |transfer_|. Only TransferParamCount() of these parameters are used, all
  // others must be zero.
  // - CUSTOM and CUSTOM_HDR: Entries A through G of the skcms_TransferFunction
  //   structure in alphabetical order.
  // - SMPTEST2084: SDR white point.
  float transfer_params_[7] = {0};

  friend struct IPC::ParamTraits<gfx::ColorSpace>;
  friend struct mojo::StructTraits<gfx::mojom::ColorSpaceDataView,
                                   gfx::ColorSpace>;
  friend struct mojo::StructTraits<media::stable::mojom::ColorSpaceDataView,
                                   gfx::ColorSpace>;
};

// Stream operator so ColorSpace can be used in assertion statements.
COLOR_SPACE_EXPORT std::ostream& operator<<(std::ostream& out,
                                            const ColorSpace& color_space);

}  // namespace gfx

#endif  // UI_GFX_COLOR_SPACE_H_
