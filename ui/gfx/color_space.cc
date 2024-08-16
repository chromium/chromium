// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/color_space.h"

#include <iomanip>
#include <limits>
#include <map>
#include <sstream>

#include "base/atomic_sequence_num.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "skia/ext/skcolorspace_primaries.h"
#include "skia/ext/skcolorspace_trfn.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"

namespace gfx {

namespace {

static bool FloatsEqualWithinTolerance(const float* a,
                                       const float* b,
                                       int n,
                                       float tol) {
  for (int i = 0; i < n; ++i) {
    if (std::abs(a[i] - b[i]) > tol) {
      return false;
    }
  }
  return true;
}

skcms_TransferFunction GetPQSkTransferFunction(float sdr_white_level) {
  // Note that SkColorSpace doesn't have the notion of an unspecified SDR white
  // level.
  if (sdr_white_level == 0.f)
    sdr_white_level = ColorSpace::kDefaultSDRWhiteLevel;

  // The generic PQ transfer function produces normalized luminance values i.e.
  // the range 0-1 represents 0-10000 nits for the reference display, but we
  // want to map 1.0 to |sdr_white_level| nits so we need to scale accordingly.
  const double w = 10000. / sdr_white_level;
  // Distribute scaling factor W by scaling A and B with X ^ (1/F):
  // ((A + Bx^C) / (D + Ex^C))^F * W = ((A + Bx^C) / (D + Ex^C) * W^(1/F))^F
  // See https://crbug.com/1058580#c32 for discussion.
  skcms_TransferFunction fn = SkNamedTransferFn::kPQ;
  const double ws = pow(w, 1. / fn.f);
  fn.a = ws * fn.a;
  fn.b = ws * fn.b;
  return fn;
}

skcms_TransferFunction GetHLGSkTransferFunction(float sdr_white_level) {
  // Note that SkColorSpace doesn't have the notion of an unspecified SDR white
  // level.
  if (sdr_white_level == 0.f)
    sdr_white_level = ColorSpace::kDefaultSDRWhiteLevel;

  // The kHLG constant will evaluate to values in the range [0, 12].
  skcms_TransferFunction fn = SkNamedTransferFn::kHLG;

  // The value of k is equal to kHLG evaluated at 0.75 (3.77) , divided by kHLG
  // evaluated at 1 (12), multiplied by 203 nits. This value is selected such
  // that a signal of 0.75 will map to the same value that a PQ signal for 203
  // nits will map to.
  constexpr float k = 63.84549817071231f;
  fn.f = k / sdr_white_level - 1;
  return fn;
}

bool PrimaryIdContainsSRGB(ColorSpace::PrimaryID id) {
  DCHECK(id != ColorSpace::PrimaryID::INVALID &&
         id != ColorSpace::PrimaryID::CUSTOM);

  switch (id) {
    case ColorSpace::PrimaryID::BT709:
    case ColorSpace::PrimaryID::BT2020:
    case ColorSpace::PrimaryID::SMPTEST428_1:
    case ColorSpace::PrimaryID::SMPTEST431_2:
    case ColorSpace::PrimaryID::P3:
    case ColorSpace::PrimaryID::XYZ_D50:
    case ColorSpace::PrimaryID::ADOBE_RGB:
    case ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN:
      return true;
    default:
      return false;
  }
}

float GetSDRWhiteLevelFromPQSkTransferFunction(
    const skcms_TransferFunction& fn) {
  DCHECK_EQ(fn.g, SkNamedTransferFn::kPQ.g);
  const double ws_a = static_cast<double>(fn.a) / SkNamedTransferFn::kPQ.a;
  const double w_a = pow(ws_a, fn.f);
  const double sdr_white_level_a = 10000.0f / w_a;
  return sdr_white_level_a;
}

}  // namespace

// static
constexpr float ColorSpace::kDefaultSDRWhiteLevel;

ColorSpace::ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range,
                       const skcms_Matrix3x3* custom_primary_matrix,
                       const skcms_TransferFunction* custom_transfer_fn)
    : primaries_(primaries),
      transfer_(transfer),
      matrix_(matrix),
      range_(range) {
  if (custom_primary_matrix) {
    DCHECK_EQ(PrimaryID::CUSTOM, primaries_);
    SetCustomPrimaries(*custom_primary_matrix);
  }
  if (custom_transfer_fn) {
    SetCustomTransferFunction(*custom_transfer_fn);
  }
}

ColorSpace::ColorSpace(const SkColorSpace& sk_color_space, bool is_hdr)
    : ColorSpace(PrimaryID::INVALID,
                 TransferID::INVALID,
                 MatrixID::RGB,
                 RangeID::FULL) {
  skcms_TransferFunction fn;
  if (sk_color_space.isNumericalTransferFn(&fn)) {
    transfer_ = is_hdr ? TransferID::CUSTOM_HDR : TransferID::CUSTOM;
    SetCustomTransferFunction(fn);
  } else if (skcms_TransferFunction_isHLGish(&fn)) {
    transfer_ = TransferID::HLG;
  } else if (skcms_TransferFunction_isPQish(&fn)) {
    transfer_ = TransferID::PQ;
    transfer_params_[0] = GetSDRWhiteLevelFromPQSkTransferFunction(fn);
  } else {
    // Construct an invalid result: Unable to extract necessary parameters
    return;
  }

  skcms_Matrix3x3 to_XYZD50;
  if (!sk_color_space.toXYZD50(&to_XYZD50)) {
    // Construct an invalid result: Unable to extract necessary parameters
    return;
  }
  SetCustomPrimaries(to_XYZD50);
}

bool ColorSpace::IsValid() const {
  return primaries_ != PrimaryID::INVALID && transfer_ != TransferID::INVALID &&
         matrix_ != MatrixID::INVALID && range_ != RangeID::INVALID;
}

// static
ColorSpace ColorSpace::CreateExtendedSRGB10Bit() {
  return ColorSpace(PrimaryID::P3, TransferID::CUSTOM_HDR, MatrixID::RGB,
                    RangeID::FULL, nullptr,
                    &SkNamedTransferFnExt::kSRGBExtended1023Over510);
}

// static
ColorSpace ColorSpace::CreatePiecewiseHDR(
    PrimaryID primaries,
    float sdr_joint,
    float hdr_level,
    const skcms_Matrix3x3* custom_primary_matrix) {
  // If |sdr_joint| is 1, then this is just sRGB (and so |hdr_level| must be 1).
  // An |sdr_joint| higher than 1 breaks.
  DCHECK_LE(sdr_joint, 1.f);
  if (sdr_joint == 1.f)
    DCHECK_EQ(hdr_level, 1.f);
  // An |hdr_level| of 1 has no HDR. An |hdr_level| less than 1 breaks.
  DCHECK_GE(hdr_level, 1.f);
  ColorSpace result(primaries, TransferID::PIECEWISE_HDR, MatrixID::RGB,
                    RangeID::FULL, custom_primary_matrix, nullptr);
  result.transfer_params_[0] = sdr_joint;
  result.transfer_params_[1] = hdr_level;
  return result;
}

// static
ColorSpace ColorSpace::CreateCustom(const skcms_Matrix3x3& to_XYZD50,
                                    const skcms_TransferFunction& fn) {
  ColorSpace result(ColorSpace::PrimaryID::CUSTOM,
                    ColorSpace::TransferID::CUSTOM, ColorSpace::MatrixID::RGB,
                    ColorSpace::RangeID::FULL, &to_XYZD50, &fn);
  return result;
}

// static
ColorSpace ColorSpace::CreateCustom(const skcms_Matrix3x3& to_XYZD50,
                                    TransferID transfer) {
  ColorSpace result(ColorSpace::PrimaryID::CUSTOM, transfer,
                    ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL,
                    &to_XYZD50, nullptr);
  return result;
}

void ColorSpace::SetCustomPrimaries(const skcms_Matrix3x3& to_XYZD50) {
  const PrimaryID kIDsToCheck[] = {
      PrimaryID::BT709,
      PrimaryID::BT470M,
      PrimaryID::BT470BG,
      PrimaryID::SMPTE170M,
      PrimaryID::SMPTE240M,
      PrimaryID::FILM,
      PrimaryID::BT2020,
      PrimaryID::SMPTEST428_1,
      PrimaryID::SMPTEST431_2,
      PrimaryID::P3,
      PrimaryID::XYZ_D50,
      PrimaryID::ADOBE_RGB,
      PrimaryID::APPLE_GENERIC_RGB,
      PrimaryID::WIDE_GAMUT_COLOR_SPIN,
      PrimaryID::EBU_3213_E,
  };
  for (PrimaryID id : kIDsToCheck) {
    skcms_Matrix3x3 matrix;
    GetPrimaryMatrix(id, &matrix);
    if (FloatsEqualWithinTolerance(&to_XYZD50.vals[0][0], &matrix.vals[0][0], 9,
                                   0.001f)) {
      primaries_ = id;
      return;
    }
  }

  memcpy(custom_primary_matrix_, &to_XYZD50, 9 * sizeof(float));
  primaries_ = PrimaryID::CUSTOM;
}

void ColorSpace::SetCustomTransferFunction(const skcms_TransferFunction& fn) {
  DCHECK(transfer_ == TransferID::CUSTOM ||
         transfer_ == TransferID::CUSTOM_HDR);

  auto check_transfer_fn = [this, &fn](TransferID id) {
    skcms_TransferFunction id_fn;
    GetTransferFunction(id, &id_fn);
    if (!FloatsEqualWithinTolerance(&fn.g, &id_fn.g, 7, 0.001f)) {
      return false;
    }
    transfer_ = id;
    return true;
  };

  if (transfer_ == TransferID::CUSTOM) {
    // These are all TransferIDs that will return a transfer function from
    // GetTransferFunction. When multiple ids map to the same function, this
    // list prioritizes the most common name (eg SRGB).
    const TransferID kIDsToCheck[] = {
        TransferID::SRGB,         TransferID::LINEAR,
        TransferID::GAMMA18,      TransferID::GAMMA22,
        TransferID::GAMMA24,      TransferID::GAMMA28,
        TransferID::SMPTE240M,    TransferID::BT709_APPLE,
        TransferID::SMPTEST428_1,
    };
    for (TransferID id : kIDsToCheck) {
      if (check_transfer_fn(id))
        return;
    }
  }

  if (transfer_ == TransferID::CUSTOM_HDR) {
    // This list is the same as above, but for HDR TransferIDs.
    const TransferID kIDsToCheckHDR[] = {
        TransferID::SRGB_HDR,
        TransferID::LINEAR_HDR,
    };
    for (TransferID id : kIDsToCheckHDR) {
      if (check_transfer_fn(id)) {
        return;
      }
    }
  }

  transfer_params_[0] = fn.a;
  transfer_params_[1] = fn.b;
  transfer_params_[2] = fn.c;
  transfer_params_[3] = fn.d;
  transfer_params_[4] = fn.e;
  transfer_params_[5] = fn.f;
  transfer_params_[6] = fn.g;
}

// static
size_t ColorSpace::TransferParamCount(TransferID transfer) {
  switch (transfer) {
    case TransferID::CUSTOM:
      return 7;
    case TransferID::CUSTOM_HDR:
      return 7;
    case TransferID::PIECEWISE_HDR:
      return 2;
    case TransferID::PQ:
      return 1;
    default:
      return 0;
  }
}

bool ColorSpace::operator==(const ColorSpace& other) const {
  if (primaries_ != other.primaries_ || transfer_ != other.transfer_ ||
      matrix_ != other.matrix_ || range_ != other.range_) {
    return false;
  }
  if (primaries_ == PrimaryID::CUSTOM) {
    if (memcmp(custom_primary_matrix_, other.custom_primary_matrix_,
               sizeof(custom_primary_matrix_))) {
      return false;
    }
  }
  if (size_t param_count = TransferParamCount(transfer_)) {
    if (memcmp(transfer_params_, other.transfer_params_,
               param_count * sizeof(float))) {
      return false;
    }
  }
  return true;
}

bool ColorSpace::IsWide() const {
  // These HDR transfer functions are always wide
  if (transfer_ == TransferID::SRGB_HDR ||
      transfer_ == TransferID::LINEAR_HDR ||
      transfer_ == TransferID::CUSTOM_HDR)
    return true;

  if (primaries_ == PrimaryID::BT2020 ||
      primaries_ == PrimaryID::SMPTEST431_2 || primaries_ == PrimaryID::P3 ||
      primaries_ == PrimaryID::ADOBE_RGB ||
      primaries_ == PrimaryID::WIDE_GAMUT_COLOR_SPIN ||
      // TODO(cblume/ccameron): Compute if the custom primaries actually are
      // wide. For now, assume so.
      primaries_ == PrimaryID::CUSTOM)
    return true;

  return false;
}

bool ColorSpace::IsHDR() const {
  return transfer_ == TransferID::PQ || transfer_ == TransferID::HLG ||
         transfer_ == TransferID::LINEAR_HDR ||
         transfer_ == TransferID::SRGB_HDR ||
         transfer_ == TransferID::CUSTOM_HDR ||
         transfer_ == TransferID::PIECEWISE_HDR ||
         transfer_ == TransferID::SCRGB_LINEAR_80_NITS;
}

bool ColorSpace::IsToneMappedByDefault() const {
  switch (transfer_) {
    case TransferID::PQ:
    case TransferID::HLG:
      return true;
    default:
      return false;
  }
}

bool ColorSpace::IsAffectedBySDRWhiteLevel() const {
  switch (transfer_) {
    case TransferID::PQ:
    case TransferID::HLG:
    case TransferID::SCRGB_LINEAR_80_NITS:
      return true;
    default:
      return false;
  }
}

bool ColorSpace::FullRangeEncodedValues() const {
  return transfer_ == TransferID::LINEAR_HDR ||
         transfer_ == TransferID::SRGB_HDR ||
         transfer_ == TransferID::CUSTOM_HDR ||
         transfer_ == TransferID::PIECEWISE_HDR ||
         transfer_ == TransferID::SCRGB_LINEAR_80_NITS ||
         transfer_ == TransferID::BT1361_ECG ||
         transfer_ == TransferID::IEC61966_2_4;
}

bool ColorSpace::operator!=(const ColorSpace& other) const {
  return !(*this == other);
}

bool ColorSpace::operator<(const ColorSpace& other) const {
  if (primaries_ < other.primaries_)
    return true;
  if (primaries_ > other.primaries_)
    return false;
  if (transfer_ < other.transfer_)
    return true;
  if (transfer_ > other.transfer_)
    return false;
  if (matrix_ < other.matrix_)
    return true;
  if (matrix_ > other.matrix_)
    return false;
  if (range_ < other.range_)
    return true;
  if (range_ > other.range_)
    return false;
  if (primaries_ == PrimaryID::CUSTOM) {
    int primary_result =
        memcmp(custom_primary_matrix_, other.custom_primary_matrix_,
               sizeof(custom_primary_matrix_));
    if (primary_result < 0)
      return true;
    if (primary_result > 0)
      return false;
  }
  if (size_t param_count = TransferParamCount(transfer_)) {
    int transfer_result = memcmp(transfer_params_, other.transfer_params_,
                                 param_count * sizeof(float));
    if (transfer_result < 0)
      return true;
    if (transfer_result > 0)
      return false;
  }
  return false;
}

size_t ColorSpace::GetHash() const {
  size_t result = (static_cast<size_t>(primaries_) << 0) |
                  (static_cast<size_t>(transfer_) << 8) |
                  (static_cast<size_t>(matrix_) << 16) |
                  (static_cast<size_t>(range_) << 24);
  if (primaries_ == PrimaryID::CUSTOM) {
    const uint32_t* params =
        reinterpret_cast<const uint32_t*>(custom_primary_matrix_);
    result ^= params[0];
    result ^= params[4];
    result ^= params[8];
  }
  {
    // Note that |transfer_params_| must be zero when they are unused.
    const uint32_t* params =
        reinterpret_cast<const uint32_t*>(transfer_params_);
    result ^= params[3];
    result ^= params[6];
  }
  return result;
}

#define PRINT_ENUM_CASE(TYPE, NAME) \
  case TYPE::NAME:                  \
    ss << #NAME;                    \
    break;

std::string ColorSpace::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4);
  if (primaries_ != PrimaryID::CUSTOM)
    ss << "{primaries:";
  switch (primaries_) {
    PRINT_ENUM_CASE(PrimaryID, INVALID)
    PRINT_ENUM_CASE(PrimaryID, BT709)
    PRINT_ENUM_CASE(PrimaryID, BT470M)
    PRINT_ENUM_CASE(PrimaryID, BT470BG)
    PRINT_ENUM_CASE(PrimaryID, SMPTE170M)
    PRINT_ENUM_CASE(PrimaryID, SMPTE240M)
    PRINT_ENUM_CASE(PrimaryID, FILM)
    PRINT_ENUM_CASE(PrimaryID, BT2020)
    PRINT_ENUM_CASE(PrimaryID, SMPTEST428_1)
    PRINT_ENUM_CASE(PrimaryID, SMPTEST431_2)
    PRINT_ENUM_CASE(PrimaryID, P3)
    PRINT_ENUM_CASE(PrimaryID, XYZ_D50)
    PRINT_ENUM_CASE(PrimaryID, ADOBE_RGB)
    PRINT_ENUM_CASE(PrimaryID, APPLE_GENERIC_RGB)
    PRINT_ENUM_CASE(PrimaryID, WIDE_GAMUT_COLOR_SPIN)
    PRINT_ENUM_CASE(PrimaryID, EBU_3213_E)
    case PrimaryID::CUSTOM:
      ss << skia::SkColorSpacePrimariesToString(GetPrimaries());
      break;
  }
  ss << ", transfer:";
  switch (transfer_) {
    PRINT_ENUM_CASE(TransferID, INVALID)
    PRINT_ENUM_CASE(TransferID, BT709)
    PRINT_ENUM_CASE(TransferID, BT709_APPLE)
    PRINT_ENUM_CASE(TransferID, GAMMA18)
    PRINT_ENUM_CASE(TransferID, GAMMA22)
    PRINT_ENUM_CASE(TransferID, GAMMA24)
    PRINT_ENUM_CASE(TransferID, GAMMA28)
    PRINT_ENUM_CASE(TransferID, SMPTE170M)
    PRINT_ENUM_CASE(TransferID, SMPTE240M)
    PRINT_ENUM_CASE(TransferID, LINEAR)
    PRINT_ENUM_CASE(TransferID, LOG)
    PRINT_ENUM_CASE(TransferID, LOG_SQRT)
    PRINT_ENUM_CASE(TransferID, IEC61966_2_4)
    PRINT_ENUM_CASE(TransferID, BT1361_ECG)
    PRINT_ENUM_CASE(TransferID, SRGB)
    PRINT_ENUM_CASE(TransferID, BT2020_10)
    PRINT_ENUM_CASE(TransferID, BT2020_12)
    PRINT_ENUM_CASE(TransferID, SMPTEST428_1)
    PRINT_ENUM_CASE(TransferID, SRGB_HDR)
    PRINT_ENUM_CASE(TransferID, LINEAR_HDR)
    case TransferID::HLG:
      ss << "HLG (SDR white point ";
      if (transfer_params_[0] == 0.f)
        ss << "default " << kDefaultSDRWhiteLevel;
      else
        ss << transfer_params_[0];
      ss << " nits)";
      break;
    case TransferID::PQ:
      ss << "PQ (SDR white point ";
      if (transfer_params_[0] == 0.f)
        ss << "default " << kDefaultSDRWhiteLevel;
      else
        ss << transfer_params_[0];
      ss << " nits)";
      break;
    case TransferID::CUSTOM: {
      skcms_TransferFunction fn;
      GetTransferFunction(&fn);
      ss << skia::SkcmsTransferFunctionToString(fn);
      break;
    }
    case TransferID::CUSTOM_HDR: {
      skcms_TransferFunction fn;
      GetTransferFunction(&fn);
      if (fn.g == 1.0f && fn.a > 0.0f && fn.b == 0.0f && fn.c == 0.0f &&
          fn.d == 0.0f && fn.e == 0.0f && fn.f == 0.0f) {
        ss << "LINEAR_HDR (slope " << fn.a << ")";
        break;
      }
      ss << skia::SkcmsTransferFunctionToString(fn);
      break;
    }
    case TransferID::PIECEWISE_HDR: {
      skcms_TransferFunction fn;
      GetTransferFunction(&fn);
      ss << "sRGB to 1 at " << transfer_params_[0] << ", linear to "
         << transfer_params_[1] << " at 1";
      break;
    }
    case TransferID::SCRGB_LINEAR_80_NITS:
      ss << "scRGB linear (80 nit white)";
      break;
  }
  ss << ", matrix:";
  switch (matrix_) {
    PRINT_ENUM_CASE(MatrixID, INVALID)
    PRINT_ENUM_CASE(MatrixID, RGB)
    PRINT_ENUM_CASE(MatrixID, BT709)
    PRINT_ENUM_CASE(MatrixID, FCC)
    PRINT_ENUM_CASE(MatrixID, BT470BG)
    PRINT_ENUM_CASE(MatrixID, SMPTE170M)
    PRINT_ENUM_CASE(MatrixID, SMPTE240M)
    PRINT_ENUM_CASE(MatrixID, YCOCG)
    PRINT_ENUM_CASE(MatrixID, BT2020_NCL)
    PRINT_ENUM_CASE(MatrixID, YDZDX)
    PRINT_ENUM_CASE(MatrixID, GBR)
  }
  ss << ", range:";
  switch (range_) {
    PRINT_ENUM_CASE(RangeID, INVALID)
    PRINT_ENUM_CASE(RangeID, LIMITED)
    PRINT_ENUM_CASE(RangeID, FULL)
    PRINT_ENUM_CASE(RangeID, DERIVED)
  }
  ss << "}";
  return ss.str();
}

#undef PRINT_ENUM_CASE

ColorSpace ColorSpace::GetAsFullRangeRGB() const {
  ColorSpace result(*this);
  if (!IsValid())
    return result;
  result.matrix_ = MatrixID::RGB;
  result.range_ = RangeID::FULL;
  return result;
}

ContentColorUsage ColorSpace::GetContentColorUsage() const {
  if (IsHDR())
    return ContentColorUsage::kHDR;
  if (IsWide())
    return ContentColorUsage::kWideColorGamut;
  return ContentColorUsage::kSRGB;
}

ColorSpace ColorSpace::GetAsRGB() const {
  ColorSpace result(*this);
  if (IsValid())
    result.matrix_ = MatrixID::RGB;
  return result;
}

ColorSpace ColorSpace::GetScaledColorSpace(float factor) const {
  ColorSpace result(*this);
  skcms_Matrix3x3 to_XYZD50;
  GetPrimaryMatrix(&to_XYZD50);
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      to_XYZD50.vals[row][col] *= factor;
    }
  }
  result.SetCustomPrimaries(to_XYZD50);
  return result;
}

bool ColorSpace::IsSuitableForBlending() const {
  switch (transfer_) {
    case TransferID::PQ:
      // PQ is not an acceptable space to do blending in -- blending 0 and 1
      // evenly will get a result of sRGB 0.259 (instead of 0.5).
      return false;
    case TransferID::HLG:
    case TransferID::LINEAR_HDR:
    case TransferID::SCRGB_LINEAR_80_NITS:
      // If the color space is nearly-linear, then it is not suitable for
      // blending -- blending 0 and 1 evenly will get a result of sRGB 0.735
      // (instead of 0.5).
      return false;
    case TransferID::CUSTOM_HDR: {
      // A gamma close enough to linear is treated as linear.
      skcms_TransferFunction fn;
      if (GetTransferFunction(&fn)) {
        constexpr float kMinGamma = 1.25;
        if (fn.g < kMinGamma)
          return false;
      }
      break;
    }
    default:
      break;
  }
  return true;
}

ColorSpace ColorSpace::GetWithMatrixAndRange(MatrixID matrix,
                                             RangeID range) const {
  ColorSpace result(*this);
  if (!IsValid())
    return result;

  result.matrix_ = matrix;
  result.range_ = range;
  return result;
}

ColorSpace ColorSpace::GetWithSdrWhiteLevel(float sdr_white_level) const {
  if (!IsAffectedBySDRWhiteLevel())
    return *this;

  return gfx::ColorSpace(*ToSkColorSpace(sdr_white_level), /*is_hdr=*/true);
}

sk_sp<SkColorSpace> ColorSpace::ToSkColorSpace(
    std::optional<float> sdr_white_level) const {
  // Handle only valid, full-range RGB spaces.
  if (!IsValid() || matrix_ != MatrixID::RGB || range_ != RangeID::FULL)
    return nullptr;

  // Use the named SRGB and linear-SRGB instead of the generic constructors.
  if (primaries_ == PrimaryID::BT709) {
    if (transfer_ == TransferID::SRGB)
      return SkColorSpace::MakeSRGB();
    if (transfer_ == TransferID::LINEAR || transfer_ == TransferID::LINEAR_HDR)
      return SkColorSpace::MakeSRGBLinear();
  }

  skcms_TransferFunction transfer_fn = SkNamedTransferFnExt::kSRGB;
  switch (transfer_) {
    case TransferID::SRGB:
      break;
    case TransferID::LINEAR:
    case TransferID::LINEAR_HDR:
      transfer_fn = SkNamedTransferFn::kLinear;
      break;
    case TransferID::HLG:
      transfer_fn = GetHLGSkTransferFunction(
          sdr_white_level.value_or(kDefaultSDRWhiteLevel));
      break;
    case TransferID::PQ:
      transfer_fn = GetPQSkTransferFunction(
          sdr_white_level.value_or(transfer_params_[0]));
      break;
    default:
      if (!GetTransferFunction(&transfer_fn, sdr_white_level)) {
        DLOG(ERROR) << "Failed to get transfer function for SkColorSpace";
        return nullptr;
      }
      break;
  }
  skcms_Matrix3x3 gamut = SkNamedGamut::kSRGB;
  switch (primaries_) {
    case PrimaryID::BT709:
      break;
    case PrimaryID::ADOBE_RGB:
      gamut = SkNamedGamut::kAdobeRGB;
      break;
    case PrimaryID::P3:
      gamut = SkNamedGamut::kDisplayP3;
      break;
    case PrimaryID::BT2020:
      gamut = SkNamedGamut::kRec2020;
      break;
    default:
      GetPrimaryMatrix(&gamut);
      break;
  }
  sk_sp<SkColorSpace> sk_color_space =
      SkColorSpace::MakeRGB(transfer_fn, gamut);
  if (!sk_color_space)
    DLOG(ERROR) << "SkColorSpace::MakeRGB failed.";

  return sk_color_space;
}

const struct _GLcolorSpace* ColorSpace::AsGLColorSpace() const {
  return reinterpret_cast<const struct _GLcolorSpace*>(this);
}

ColorSpace::PrimaryID ColorSpace::GetPrimaryID() const {
  return primaries_;
}

ColorSpace::TransferID ColorSpace::GetTransferID() const {
  return transfer_;
}

ColorSpace::MatrixID ColorSpace::GetMatrixID() const {
  return matrix_;
}

ColorSpace::RangeID ColorSpace::GetRangeID() const {
  return range_;
}

bool ColorSpace::HasExtendedSkTransferFn() const {
  return matrix_ == MatrixID::RGB;
}

bool ColorSpace::IsTransferFunctionEqualTo(
    const skcms_TransferFunction& fn) const {
  if (transfer_ == TransferID::PQ)
    return skcms_TransferFunction_isPQish(&fn);
  if (transfer_ == TransferID::HLG)
    return skcms_TransferFunction_isHLGish(&fn);
  if (!skcms_TransferFunction_isSRGBish(&fn))
    return false;
  skcms_TransferFunction transfer_fn;
  GetTransferFunction(&transfer_fn);
  return fn.a == transfer_fn.a && fn.b == transfer_fn.b &&
         fn.c == transfer_fn.c && fn.d == transfer_fn.d &&
         fn.e == transfer_fn.e && fn.f == transfer_fn.f &&
         fn.g == transfer_fn.g;
}

bool ColorSpace::Contains(const ColorSpace& other) const {
  if (primaries_ == PrimaryID::INVALID ||
      other.primaries_ == PrimaryID::INVALID)
    return false;

  // Contains() is commonly used to check if a color space contains sRGB. The
  // computation can be bypassed for known primary IDs.
  if (primaries_ != PrimaryID::CUSTOM && other.primaries_ == PrimaryID::BT709)
    return PrimaryIdContainsSRGB(primaries_);

  // |matrix| is the primary transform matrix from |other| to this color space.
  skcms_Matrix3x3 other_to_xyz;
  skcms_Matrix3x3 this_to_xyz;
  skcms_Matrix3x3 xyz_to_this;
  other.GetPrimaryMatrix(&other_to_xyz);
  GetPrimaryMatrix(&this_to_xyz);
  skcms_Matrix3x3_invert(&this_to_xyz, &xyz_to_this);
  skcms_Matrix3x3 matrix = skcms_Matrix3x3_concat(&xyz_to_this, &other_to_xyz);

  // Return true iff each primary is in the range [0, 1] after transforming.
  // Transforming a primary vector by |matrix| always results in a column of
  // |matrix|. So the multiplication can be skipped, and we can just check if
  // each value in the matrix is in the range [0, 1].
  constexpr float epsilon = 0.001f;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      if (matrix.vals[r][c] < -epsilon || matrix.vals[r][c] > 1 + epsilon)
        return false;
    }
  }
  return true;
}

// static
SkColorSpacePrimaries ColorSpace::GetColorSpacePrimaries(
    PrimaryID primary_id,
    const skcms_Matrix3x3* custom_primary_matrix = nullptr) {
  SkColorSpacePrimaries primaries = SkNamedPrimariesExt::kInvalid;

  if (custom_primary_matrix && primary_id == PrimaryID::CUSTOM)
    return skia::GetD65PrimariesFromToXYZD50Matrix(*custom_primary_matrix);

  switch (primary_id) {
    case ColorSpace::PrimaryID::CUSTOM:
    case ColorSpace::PrimaryID::INVALID:
      break;

    case ColorSpace::PrimaryID::BT709:
      // BT709 is our default case. Put it after the switch just
      // in case we somehow get an id which is not listed in the switch.
      // (We don't want to use "default", because we want the compiler
      //  to tell us if we forgot some enum values.)
      return SkNamedPrimariesExt::kRec709;

    case ColorSpace::PrimaryID::BT470M:
      return SkNamedPrimariesExt::kRec470SystemM;

    case ColorSpace::PrimaryID::BT470BG:
      return SkNamedPrimariesExt::kRec470SystemBG;

    case ColorSpace::PrimaryID::SMPTE170M:
      return SkNamedPrimariesExt::kRec601;

    case ColorSpace::PrimaryID::SMPTE240M:
      return SkNamedPrimariesExt::kSMPTE_ST_240;

    case ColorSpace::PrimaryID::APPLE_GENERIC_RGB:
      return SkNamedPrimariesExt::kAppleGenericRGB;

    case ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN:
      return SkNamedPrimariesExt::kWideGamutColorSpin;

    case ColorSpace::PrimaryID::FILM:
      return SkNamedPrimariesExt::kGenericFilm;

    case ColorSpace::PrimaryID::BT2020:
      return SkNamedPrimariesExt::kRec2020;

    case ColorSpace::PrimaryID::SMPTEST428_1:
      return SkNamedPrimariesExt::kSMPTE_ST_428_1;

    case ColorSpace::PrimaryID::SMPTEST431_2:
      return SkNamedPrimariesExt::kSMPTE_RP_431_2;

    case ColorSpace::PrimaryID::P3:
      return SkNamedPrimariesExt::kP3;

    case ColorSpace::PrimaryID::XYZ_D50:
      return SkNamedPrimariesExt::kXYZD50;

    case ColorSpace::PrimaryID::ADOBE_RGB:
      return SkNamedPrimariesExt::kA98RGB;

    case ColorSpace::PrimaryID::EBU_3213_E:
      return SkNamedPrimariesExt::kITU_T_H273_Value22;
  }
  return primaries;
}

SkColorSpacePrimaries ColorSpace::GetPrimaries() const {
  skcms_Matrix3x3 matrix;
  memcpy(&matrix, custom_primary_matrix_, 9 * sizeof(float));
  return GetColorSpacePrimaries(primaries_, &matrix);
}

// static
void ColorSpace::GetPrimaryMatrix(PrimaryID primary_id,
                                  skcms_Matrix3x3* to_XYZD50) {
  SkColorSpacePrimaries primaries = GetColorSpacePrimaries(primary_id);

  if (primary_id == PrimaryID::CUSTOM || primary_id == PrimaryID::INVALID) {
    *to_XYZD50 = SkNamedGamut::kXYZ;  // Identity
    return;
  }
  primaries.toXYZD50(to_XYZD50);
}

void ColorSpace::GetPrimaryMatrix(skcms_Matrix3x3* to_XYZD50) const {
  if (primaries_ == PrimaryID::CUSTOM) {
    memcpy(to_XYZD50, custom_primary_matrix_, 9 * sizeof(float));
  } else {
    GetPrimaryMatrix(primaries_, to_XYZD50);
  }
}

SkM44 ColorSpace::GetPrimaryMatrix() const {
  skcms_Matrix3x3 toXYZ_3x3;
  GetPrimaryMatrix(&toXYZ_3x3);
  return SkM44FromSkcmsMatrix3x3(toXYZ_3x3);
}

// static
bool ColorSpace::GetTransferFunction(TransferID transfer,
                                     skcms_TransferFunction* fn) {
  // Default to F(x) = pow(x, 1)
  fn->a = 1;
  fn->b = 0;
  fn->c = 0;
  fn->d = 0;
  fn->e = 0;
  fn->f = 0;
  fn->g = 1;

  switch (transfer) {
    case ColorSpace::TransferID::LINEAR:
    case ColorSpace::TransferID::LINEAR_HDR:
      *fn = SkNamedTransferFn::kLinear;
      return true;
    case ColorSpace::TransferID::GAMMA18:
      fn->g = 1.801f;
      return true;
    case ColorSpace::TransferID::GAMMA22:
      *fn = SkNamedTransferFnExt::kRec470SystemM;
      return true;
    case ColorSpace::TransferID::GAMMA24:
      fn->g = 2.4f;
      return true;
    case ColorSpace::TransferID::GAMMA28:
      *fn = SkNamedTransferFnExt::kRec470SystemBG;
      return true;
    case ColorSpace::TransferID::SMPTE240M:
      *fn = SkNamedTransferFnExt::kSMPTE_ST_240;
      return true;
    case ColorSpace::TransferID::BT709:
    case ColorSpace::TransferID::SMPTE170M:
    case ColorSpace::TransferID::BT2020_10:
    case ColorSpace::TransferID::BT2020_12:
    // With respect to rendering BT709
    //  * SMPTE 1886 suggests that we should be using gamma 2.4.
    //  * Most displays actually use a gamma of 2.2, and most media playing
    //    software uses the sRGB transfer function.
    //  * User studies shows that users don't really care.
    //  * Apple's CoreVideo uses gamma=1.961.
    // Bearing all of that in mind, use the same transfer function as sRGB,
    // which will allow more optimization, and will more closely match other
    // media players.
    case ColorSpace::TransferID::SRGB:
    case ColorSpace::TransferID::SRGB_HDR:
      *fn = SkNamedTransferFnExt::kSRGB;
      return true;
    case ColorSpace::TransferID::BT709_APPLE:
      *fn = SkNamedTransferFnExt::kRec709Apple;
      return true;
    case ColorSpace::TransferID::SMPTEST428_1:
      *fn = SkNamedTransferFnExt::kSMPTE_ST_428_1;
      return true;
    case ColorSpace::TransferID::IEC61966_2_4:
      // This could potentially be represented the same as SRGB, but it handles
      // negative values differently.
      break;
    case ColorSpace::TransferID::HLG:
    case ColorSpace::TransferID::BT1361_ECG:
    case ColorSpace::TransferID::LOG:
    case ColorSpace::TransferID::LOG_SQRT:
    case ColorSpace::TransferID::PQ:
    case ColorSpace::TransferID::CUSTOM:
    case ColorSpace::TransferID::CUSTOM_HDR:
    case ColorSpace::TransferID::PIECEWISE_HDR:
    case ColorSpace::TransferID::SCRGB_LINEAR_80_NITS:
    case ColorSpace::TransferID::INVALID:
      break;
  }

  return false;
}

bool ColorSpace::GetTransferFunction(
    skcms_TransferFunction* fn,
    std::optional<float> sdr_white_level) const {
  switch (transfer_) {
    case TransferID::CUSTOM:
    case TransferID::CUSTOM_HDR:
      fn->a = transfer_params_[0];
      fn->b = transfer_params_[1];
      fn->c = transfer_params_[2];
      fn->d = transfer_params_[3];
      fn->e = transfer_params_[4];
      fn->f = transfer_params_[5];
      fn->g = transfer_params_[6];
      return true;
    case TransferID::SCRGB_LINEAR_80_NITS:
      if (sdr_white_level) {
        fn->a = 80.f / *sdr_white_level;
        fn->b = 0;
        fn->c = 0;
        fn->d = 0;
        fn->e = 0;
        fn->f = 0;
        fn->g = 1;
        return true;
      } else {
        // Using SCRGB_LINEAR_80_NITS without specifying an SDR white level is
        // guaranteed to produce incorrect results.
        return false;
      }
    default:
      return GetTransferFunction(transfer_, fn);
  }
}

bool ColorSpace::GetInverseTransferFunction(
    skcms_TransferFunction* fn,
    std::optional<float> sdr_white_level) const {
  if (!GetTransferFunction(fn, sdr_white_level))
    return false;
  *fn = SkTransferFnInverse(*fn);
  return true;
}

bool ColorSpace::GetPiecewiseHDRParams(float* sdr_joint,
                                       float* hdr_level) const {
  if (transfer_ != TransferID::PIECEWISE_HDR)
    return false;
  *sdr_joint = transfer_params_[0];
  *hdr_level = transfer_params_[1];
  return true;
}

SkM44 ColorSpace::GetTransferMatrix(int bit_depth) const {
  DCHECK_GE(bit_depth, 8);
  // If chroma samples are real numbers in the range of −0.5 to 0.5, an offset
  // of 0.5 is added to get real numbers in the range of 0 to 1. When
  // represented as an unsigned |bit_depth|-bit integer, this 0.5 offset is
  // approximated by 1 << (bit_depth - 1). chroma_0_5 is this approximate value
  // converted to a real number in the range of 0 to 1.
  //
  // TODO(wtc): For now chroma_0_5 is only used for YCgCo. It should also be
  // used for YUV.
  const float chroma_0_5 =
      static_cast<float>(1 << (bit_depth - 1)) / ((1 << bit_depth) - 1);
  float Kr = 0;
  float Kb = 0;
  switch (matrix_) {
    case ColorSpace::MatrixID::RGB:
    case ColorSpace::MatrixID::INVALID:
      return SkM44();

    case ColorSpace::MatrixID::BT709:
      Kr = 0.2126f;
      Kb = 0.0722f;
      break;

    case ColorSpace::MatrixID::FCC:
      Kr = 0.30f;
      Kb = 0.11f;
      break;

    case ColorSpace::MatrixID::BT470BG:
    case ColorSpace::MatrixID::SMPTE170M:
      Kr = 0.299f;
      Kb = 0.114f;
      break;

    case ColorSpace::MatrixID::SMPTE240M:
      Kr = 0.212f;
      Kb = 0.087f;
      break;

    case ColorSpace::MatrixID::YCOCG: {
      float data[16] = {0.25f,  0.5f, 0.25f,  0.0f,        // Y
                        -0.25f, 0.5f, -0.25f, chroma_0_5,  // Cg
                        0.5f,   0.0f, -0.5f,  chroma_0_5,  // Co
                        0.0f,   0.0f, 0.0f,   1.0f};
      return SkM44::RowMajor(data);
    }

    case ColorSpace::MatrixID::BT2020_NCL:
      Kr = 0.2627f;
      Kb = 0.0593f;
      break;

    case ColorSpace::MatrixID::YDZDX: {
      // clang-format off
      float data[16] = {
          0.0f,              1.0f,             0.0f, 0.0f,  // Y
          0.0f,             -0.5f, 0.986566f / 2.0f, 0.5f,  // DX or DZ
          0.5f, -0.991902f / 2.0f,             0.0f, 0.5f,  // DZ or DX
          0.0f,              0.0f,             0.0f, 1.0f,
      };
      // clang-format on
      return SkM44::RowMajor(data);
    }
    case ColorSpace::MatrixID::GBR: {
      float data[16] = {0.0f, 1.0f, 0.0f, 0.0f,  // G
                        0.0f, 0.0f, 1.0f, 0.0f,  // B
                        1.0f, 0.0f, 0.0f, 0.0f,  // R
                        0.0f, 0.0f, 0.0f, 1.0f};
      return SkM44::RowMajor(data);
    }
  }
  float Kg = 1.0f - Kr - Kb;
  float u_m = 0.5f / (1.0f - Kb);
  float v_m = 0.5f / (1.0f - Kr);
  // clang-format off
  float data[16] = {
                     Kr,        Kg,                Kb, 0.0f,  // Y
              u_m * -Kr, u_m * -Kg, u_m * (1.0f - Kb), 0.5f,  // U
      v_m * (1.0f - Kr), v_m * -Kg,         v_m * -Kb, 0.5f,  // V
                   0.0f,      0.0f,              0.0f, 1.0f,
  };
  // clang-format on
  return SkM44::RowMajor(data);
}

SkM44 ColorSpace::GetRangeAdjustMatrix(int bit_depth) const {
  DCHECK_GE(bit_depth, 8);
  switch (range_) {
    case RangeID::FULL:
    case RangeID::INVALID:
      return SkM44();

    case RangeID::DERIVED:
    case RangeID::LIMITED:
      break;
  }

  // See ITU-T H.273 (2016), Section 8.3. The following is derived from
  // Equations 20-31.
  const int shift = bit_depth - 8;
  const float a_y = 219 << shift;
  const float c = (1 << bit_depth) - 1;
  const float scale_y = c / a_y;
  switch (matrix_) {
    case MatrixID::RGB:
    case MatrixID::GBR:
    case MatrixID::INVALID:
    case MatrixID::YCOCG:
      return SkM44::Scale(scale_y, scale_y, scale_y)
          .postTranslate(-16.0f / 219.0f, -16.0f / 219.0f, -16.0f / 219.0f);

    case MatrixID::BT709:
    case MatrixID::FCC:
    case MatrixID::BT470BG:
    case MatrixID::SMPTE170M:
    case MatrixID::SMPTE240M:
    case MatrixID::BT2020_NCL:
    case MatrixID::YDZDX: {
      const float a_uv = 224 << shift;
      const float scale_uv = c / a_uv;
      const float translate_uv = (a_uv - c) / (2.0f * a_uv);
      return SkM44::Scale(scale_y, scale_uv, scale_uv)
          .postTranslate(-16.0f / 219.0f, translate_uv, translate_uv);
    }
  }
  NOTREACHED_IN_MIGRATION();
  return SkM44();
}

bool ColorSpace::ToSkYUVColorSpace(int bit_depth, SkYUVColorSpace* out) const {
  switch (matrix_) {
    case MatrixID::BT709:
      *out = range_ == RangeID::FULL ? kRec709_Full_SkYUVColorSpace
                                     : kRec709_Limited_SkYUVColorSpace;
      return true;

    case MatrixID::BT470BG:
    case MatrixID::SMPTE170M:
      *out = range_ == RangeID::FULL ? kJPEG_SkYUVColorSpace
                                     : kRec601_Limited_SkYUVColorSpace;
      return true;

    case MatrixID::BT2020_NCL:
      if (bit_depth == 8) {
        *out = range_ == RangeID::FULL ? kBT2020_8bit_Full_SkYUVColorSpace
                                       : kBT2020_8bit_Limited_SkYUVColorSpace;
        return true;
      }
      if (bit_depth == 10) {
        *out = range_ == RangeID::FULL ? kBT2020_10bit_Full_SkYUVColorSpace
                                       : kBT2020_10bit_Limited_SkYUVColorSpace;
        return true;
      }
      if (bit_depth == 12) {
        *out = range_ == RangeID::FULL ? kBT2020_12bit_Full_SkYUVColorSpace
                                       : kBT2020_12bit_Limited_SkYUVColorSpace;
        return true;
      }
      if (bit_depth == 16) {
        *out = range_ == RangeID::FULL ? kBT2020_16bit_Full_SkYUVColorSpace
                                       : kBT2020_16bit_Limited_SkYUVColorSpace;
        return true;
      }
      return false;

    case MatrixID::FCC:
      *out = range_ == RangeID::FULL ? kFCC_Full_SkYUVColorSpace
                                     : kFCC_Limited_SkYUVColorSpace;
      return true;

    case MatrixID::SMPTE240M:
      *out = range_ == RangeID::FULL ? kSMPTE240_Full_SkYUVColorSpace
                                     : kSMPTE240_Limited_SkYUVColorSpace;
      return true;

    case MatrixID::YDZDX:
      *out = range_ == RangeID::FULL ? kYDZDX_Full_SkYUVColorSpace
                                     : kYDZDX_Limited_SkYUVColorSpace;
      return true;

    case MatrixID::GBR:
      *out = range_ == RangeID::FULL ? kGBR_Full_SkYUVColorSpace
                                     : kGBR_Limited_SkYUVColorSpace;
      return true;

    case MatrixID::YCOCG:
      if (bit_depth == 8) {
        *out = range_ == RangeID::FULL ? kYCgCo_8bit_Full_SkYUVColorSpace
                                       : kYCgCo_8bit_Limited_SkYUVColorSpace;
        return true;
      }
      if (bit_depth == 10) {
        *out = range_ == RangeID::FULL ? kYCgCo_10bit_Full_SkYUVColorSpace
                                       : kYCgCo_10bit_Limited_SkYUVColorSpace;
        return true;
      }
      if (bit_depth == 12) {
        *out = range_ == RangeID::FULL ? kYCgCo_12bit_Full_SkYUVColorSpace
                                       : kYCgCo_12bit_Limited_SkYUVColorSpace;
        return true;
      }
      if (bit_depth == 16) {
        *out = range_ == RangeID::FULL ? kYCgCo_16bit_Full_SkYUVColorSpace
                                       : kYCgCo_16bit_Limited_SkYUVColorSpace;
        return true;
      }
      return false;
    default:
      break;
  }
  return false;
}

std::ostream& operator<<(std::ostream& out, const ColorSpace& color_space) {
  return out << color_space.ToString();
}

}  // namespace gfx
