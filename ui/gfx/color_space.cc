// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space.h"

#include <iomanip>
#include <limits>
#include <map>
#include <sstream>

#include "base/atomic_sequence_num.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkICC.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"

namespace gfx {

namespace {

static bool IsAlmostZero(float value) {
  return std::abs(value) < std::numeric_limits<float>::epsilon();
}

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
  if (custom_transfer_fn)
    SetCustomTransferFunction(*custom_transfer_fn);
}

ColorSpace::ColorSpace(const SkColorSpace& sk_color_space)
    : ColorSpace(PrimaryID::INVALID,
                 TransferID::INVALID,
                 MatrixID::RGB,
                 RangeID::FULL) {
  skcms_TransferFunction fn;
  if (sk_color_space.isNumericalTransferFn(&fn)) {
    transfer_ = TransferID::CUSTOM;
    SetCustomTransferFunction(fn);
  } else if (skcms_TransferFunction_isHLGish(&fn)) {
    transfer_ = TransferID::ARIB_STD_B67;
  } else if (skcms_TransferFunction_isPQish(&fn)) {
    transfer_ = TransferID::SMPTEST2084;
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
ColorSpace ColorSpace::CreateSCRGBLinear(float sdr_white_level) {
  skcms_TransferFunction fn = {0};
  fn.g = 1.0f;
  fn.a = kDefaultScrgbLinearSdrWhiteLevel / sdr_white_level;
  return ColorSpace(PrimaryID::BT709, TransferID::CUSTOM_HDR, MatrixID::RGB,
                    RangeID::FULL, nullptr, &fn);
}

// static
ColorSpace ColorSpace::CreateHDR10(float sdr_white_level) {
  ColorSpace result(PrimaryID::BT2020, TransferID::SMPTEST2084, MatrixID::RGB,
                    RangeID::FULL);
  result.transfer_params_[0] = sdr_white_level;
  return result;
}

// static
ColorSpace ColorSpace::CreateHLG() {
  return ColorSpace(PrimaryID::BT2020, TransferID::ARIB_STD_B67, MatrixID::RGB,
                    RangeID::FULL);
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
      PrimaryID::SMPTEST432_1,
      PrimaryID::XYZ_D50,
      PrimaryID::ADOBE_RGB,
      PrimaryID::APPLE_GENERIC_RGB,
      PrimaryID::WIDE_GAMUT_COLOR_SPIN,
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
  // These are all TransferIDs that will return a transfer function from
  // GetTransferFunction. When multiple ids map to the same function, this list
  // prioritizes the most common name (eg IEC61966_2_1). This applies only to
  // SDR transfer functions.
  if (transfer_ == TransferID::CUSTOM) {
    const TransferID kIDsToCheck[] = {
        TransferID::IEC61966_2_1, TransferID::LINEAR,
        TransferID::GAMMA18,      TransferID::GAMMA22,
        TransferID::GAMMA24,      TransferID::GAMMA28,
        TransferID::SMPTE240M,    TransferID::BT709_APPLE,
        TransferID::SMPTEST428_1,
    };
    for (TransferID id : kIDsToCheck) {
      skcms_TransferFunction id_fn;
      GetTransferFunction(id, &id_fn);
      if (FloatsEqualWithinTolerance(&fn.g, &id_fn.g, 7, 0.001f)) {
        transfer_ = id;
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
    case TransferID::SMPTEST2084:
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
  if (transfer_ == TransferID::IEC61966_2_1_HDR ||
      transfer_ == TransferID::LINEAR_HDR ||
      transfer_ == TransferID::CUSTOM_HDR)
    return true;

  if (primaries_ == PrimaryID::BT2020 ||
      primaries_ == PrimaryID::SMPTEST431_2 ||
      primaries_ == PrimaryID::SMPTEST432_1 ||
      primaries_ == PrimaryID::ADOBE_RGB ||
      primaries_ == PrimaryID::WIDE_GAMUT_COLOR_SPIN ||
      // TODO(cblume/ccameron): Compute if the custom primaries actually are
      // wide. For now, assume so.
      primaries_ == PrimaryID::CUSTOM)
    return true;

  return false;
}

bool ColorSpace::IsHDR() const {
  return transfer_ == TransferID::SMPTEST2084 ||
         transfer_ == TransferID::ARIB_STD_B67 ||
         transfer_ == TransferID::LINEAR_HDR ||
         transfer_ == TransferID::IEC61966_2_1_HDR ||
         transfer_ == TransferID::CUSTOM_HDR ||
         transfer_ == TransferID::PIECEWISE_HDR;
}

bool ColorSpace::FullRangeEncodedValues() const {
  return transfer_ == TransferID::LINEAR_HDR ||
         transfer_ == TransferID::IEC61966_2_1_HDR ||
         transfer_ == TransferID::CUSTOM_HDR ||
         transfer_ == TransferID::PIECEWISE_HDR ||
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
    PRINT_ENUM_CASE(PrimaryID, SMPTEST432_1)
    PRINT_ENUM_CASE(PrimaryID, XYZ_D50)
    PRINT_ENUM_CASE(PrimaryID, ADOBE_RGB)
    PRINT_ENUM_CASE(PrimaryID, APPLE_GENERIC_RGB)
    PRINT_ENUM_CASE(PrimaryID, WIDE_GAMUT_COLOR_SPIN)
    case PrimaryID::CUSTOM:
      // |custom_primary_matrix_| is in row-major order.
      const float sum_R = custom_primary_matrix_[0] +
                          custom_primary_matrix_[3] + custom_primary_matrix_[6];
      const float sum_G = custom_primary_matrix_[1] +
                          custom_primary_matrix_[4] + custom_primary_matrix_[7];
      const float sum_B = custom_primary_matrix_[2] +
                          custom_primary_matrix_[5] + custom_primary_matrix_[8];
      if (IsAlmostZero(sum_R) || IsAlmostZero(sum_G) || IsAlmostZero(sum_B))
        break;

      ss << "{primaries_d50_referred: [[" << (custom_primary_matrix_[0] / sum_R)
         << ", " << (custom_primary_matrix_[3] / sum_R) << "], "
         << " [" << (custom_primary_matrix_[1] / sum_G) << ", "
         << (custom_primary_matrix_[4] / sum_G) << "], "
         << " [" << (custom_primary_matrix_[2] / sum_B) << ", "
         << (custom_primary_matrix_[5] / sum_B) << "]]";
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
    PRINT_ENUM_CASE(TransferID, IEC61966_2_1)
    PRINT_ENUM_CASE(TransferID, BT2020_10)
    PRINT_ENUM_CASE(TransferID, BT2020_12)
    PRINT_ENUM_CASE(TransferID, SMPTEST428_1)
    PRINT_ENUM_CASE(TransferID, ARIB_STD_B67)
    PRINT_ENUM_CASE(TransferID, IEC61966_2_1_HDR)
    PRINT_ENUM_CASE(TransferID, LINEAR_HDR)
    case TransferID::SMPTEST2084:
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
      ss << fn.c << "*x + " << fn.f << " if x < " << fn.d << " else (" << fn.a
         << "*x + " << fn.b << ")**" << fn.g << " + " << fn.e;
      break;
    }
    case TransferID::CUSTOM_HDR: {
      skcms_TransferFunction fn;
      GetTransferFunction(&fn);
      if (fn.g == 1.0f && fn.a > 0.0f && fn.b == 0.0f && fn.c == 0.0f &&
          fn.d == 0.0f && fn.e == 0.0f && fn.f == 0.0f) {
        ss << "LINEAR_HDR (slope " << fn.a << ", SDR white point "
           << kDefaultScrgbLinearSdrWhiteLevel / fn.a << " nits)";
        break;
      }
      ss << fn.c << "*x + " << fn.f << " if |x| < " << fn.d << " else sign(x)*("
         << fn.a << "*|x| + " << fn.b << ")**" << fn.g << " + " << fn.e;
      break;
    }
    case TransferID::PIECEWISE_HDR: {
      skcms_TransferFunction fn;
      GetTransferFunction(&fn);
      ss << "sRGB to 1 at " << transfer_params_[0] << ", linear to "
         << transfer_params_[1] << " at 1";
      break;
    }
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
    PRINT_ENUM_CASE(MatrixID, BT2020_CL)
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
    case TransferID::SMPTEST2084:
      // PQ is not an acceptable space to do blending in -- blending 0 and 1
      // evenly will get a result of sRGB 0.259 (instead of 0.5).
      return false;
    case TransferID::LINEAR_HDR:
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

ColorSpace ColorSpace::GetWithSDRWhiteLevel(float sdr_white_level) const {
  ColorSpace result = *this;
  if (transfer_ == TransferID::SMPTEST2084) {
    result.transfer_params_[0] = sdr_white_level;
  } else if (transfer_ == TransferID::LINEAR_HDR) {
    result.transfer_ = TransferID::CUSTOM_HDR;
    skcms_TransferFunction fn = {0};
    fn.g = 1.f;
    fn.a = kDefaultScrgbLinearSdrWhiteLevel / sdr_white_level;
    result.SetCustomTransferFunction(fn);
  }
  return result;
}

sk_sp<SkColorSpace> ColorSpace::ToSkColorSpace() const {
  // Unspecified color spaces correspond to the null SkColorSpace.
  if (!IsValid())
    return nullptr;

  // Handle only full-range RGB spaces.
  if (matrix_ != MatrixID::RGB) {
    DLOG(ERROR) << "Not creating non-RGB SkColorSpace";
    return nullptr;
  }
  if (range_ != RangeID::FULL) {
    DLOG(ERROR) << "Not creating non-full-range SkColorSpace";
    return nullptr;
  }

  // Use the named SRGB and linear-SRGB instead of the generic constructors.
  if (primaries_ == PrimaryID::BT709) {
    if (transfer_ == TransferID::IEC61966_2_1)
      return SkColorSpace::MakeSRGB();
    if (transfer_ == TransferID::LINEAR || transfer_ == TransferID::LINEAR_HDR)
      return SkColorSpace::MakeSRGBLinear();
  }

  skcms_TransferFunction transfer_fn = SkNamedTransferFn::kSRGB;
  switch (transfer_) {
    case TransferID::IEC61966_2_1:
      break;
    case TransferID::LINEAR:
    case TransferID::LINEAR_HDR:
      transfer_fn = SkNamedTransferFn::kLinear;
      break;
    case TransferID::ARIB_STD_B67:
      transfer_fn = SkNamedTransferFn::kHLG;
      break;
    case TransferID::SMPTEST2084:
      transfer_fn = GetPQSkTransferFunction(transfer_params_[0]);
      break;
    default:
      if (!GetTransferFunction(&transfer_fn)) {
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
    case PrimaryID::SMPTEST432_1:
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
  return transfer_ == TransferID::LINEAR_HDR ||
         transfer_ == TransferID::IEC61966_2_1_HDR;
}

// static
void ColorSpace::GetPrimaryMatrix(PrimaryID primary_id,
                                  skcms_Matrix3x3* to_XYZD50) {
  SkColorSpacePrimaries primaries = {0};
  switch (primary_id) {
    case ColorSpace::PrimaryID::CUSTOM:
    case ColorSpace::PrimaryID::INVALID:
      *to_XYZD50 = SkNamedGamut::kXYZ;  // Identity
      return;

    case ColorSpace::PrimaryID::BT709:
      // BT709 is our default case. Put it after the switch just
      // in case we somehow get an id which is not listed in the switch.
      // (We don't want to use "default", because we want the compiler
      //  to tell us if we forgot some enum values.)
      primaries.fRX = 0.640f;
      primaries.fRY = 0.330f;
      primaries.fGX = 0.300f;
      primaries.fGY = 0.600f;
      primaries.fBX = 0.150f;
      primaries.fBY = 0.060f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::BT470M:
      primaries.fRX = 0.67f;
      primaries.fRY = 0.33f;
      primaries.fGX = 0.21f;
      primaries.fGY = 0.71f;
      primaries.fBX = 0.14f;
      primaries.fBY = 0.08f;
      primaries.fWX = 0.31f;
      primaries.fWY = 0.316f;
      break;

    case ColorSpace::PrimaryID::BT470BG:
      primaries.fRX = 0.64f;
      primaries.fRY = 0.33f;
      primaries.fGX = 0.29f;
      primaries.fGY = 0.60f;
      primaries.fBX = 0.15f;
      primaries.fBY = 0.06f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::SMPTE170M:
    case ColorSpace::PrimaryID::SMPTE240M:
      primaries.fRX = 0.630f;
      primaries.fRY = 0.340f;
      primaries.fGX = 0.310f;
      primaries.fGY = 0.595f;
      primaries.fBX = 0.155f;
      primaries.fBY = 0.070f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::APPLE_GENERIC_RGB:
      primaries.fRX = 0.63002f;
      primaries.fRY = 0.34000f;
      primaries.fGX = 0.29505f;
      primaries.fGY = 0.60498f;
      primaries.fBX = 0.15501f;
      primaries.fBY = 0.07701f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::WIDE_GAMUT_COLOR_SPIN:
      primaries.fRX = 0.01f;
      primaries.fRY = 0.98f;
      primaries.fGX = 0.01f;
      primaries.fGY = 0.01f;
      primaries.fBX = 0.98f;
      primaries.fBY = 0.01f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::FILM:
      primaries.fRX = 0.681f;
      primaries.fRY = 0.319f;
      primaries.fGX = 0.243f;
      primaries.fGY = 0.692f;
      primaries.fBX = 0.145f;
      primaries.fBY = 0.049f;
      primaries.fWX = 0.310f;
      primaries.fWY = 0.136f;
      break;

    case ColorSpace::PrimaryID::BT2020:
      primaries.fRX = 0.708f;
      primaries.fRY = 0.292f;
      primaries.fGX = 0.170f;
      primaries.fGY = 0.797f;
      primaries.fBX = 0.131f;
      primaries.fBY = 0.046f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::SMPTEST428_1:
      primaries.fRX = 1.0f;
      primaries.fRY = 0.0f;
      primaries.fGX = 0.0f;
      primaries.fGY = 1.0f;
      primaries.fBX = 0.0f;
      primaries.fBY = 0.0f;
      primaries.fWX = 1.0f / 3.0f;
      primaries.fWY = 1.0f / 3.0f;
      break;

    case ColorSpace::PrimaryID::SMPTEST431_2:
      primaries.fRX = 0.680f;
      primaries.fRY = 0.320f;
      primaries.fGX = 0.265f;
      primaries.fGY = 0.690f;
      primaries.fBX = 0.150f;
      primaries.fBY = 0.060f;
      primaries.fWX = 0.314f;
      primaries.fWY = 0.351f;
      break;

    case ColorSpace::PrimaryID::SMPTEST432_1:
      primaries.fRX = 0.680f;
      primaries.fRY = 0.320f;
      primaries.fGX = 0.265f;
      primaries.fGY = 0.690f;
      primaries.fBX = 0.150f;
      primaries.fBY = 0.060f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;

    case ColorSpace::PrimaryID::XYZ_D50:
      primaries.fRX = 1.0f;
      primaries.fRY = 0.0f;
      primaries.fGX = 0.0f;
      primaries.fGY = 1.0f;
      primaries.fBX = 0.0f;
      primaries.fBY = 0.0f;
      primaries.fWX = 0.34567f;
      primaries.fWY = 0.35850f;
      break;

    case ColorSpace::PrimaryID::ADOBE_RGB:
      primaries.fRX = 0.6400f;
      primaries.fRY = 0.3300f;
      primaries.fGX = 0.2100f;
      primaries.fGY = 0.7100f;
      primaries.fBX = 0.1500f;
      primaries.fBY = 0.0600f;
      primaries.fWX = 0.3127f;
      primaries.fWY = 0.3290f;
      break;
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

void ColorSpace::GetPrimaryMatrix(SkMatrix44* to_XYZD50) const {
  skcms_Matrix3x3 toXYZ_3x3;
  GetPrimaryMatrix(&toXYZ_3x3);
  to_XYZD50->set3x3RowMajorf(&toXYZ_3x3.vals[0][0]);
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
      return true;
    case ColorSpace::TransferID::GAMMA18:
      fn->g = 1.801f;
      return true;
    case ColorSpace::TransferID::GAMMA22:
      fn->g = 2.2f;
      return true;
    case ColorSpace::TransferID::GAMMA24:
      fn->g = 2.4f;
      return true;
    case ColorSpace::TransferID::GAMMA28:
      fn->g = 2.8f;
      return true;
    case ColorSpace::TransferID::SMPTE240M:
      fn->a = 0.899626676224f;
      fn->b = 0.100373323776f;
      fn->c = 0.250000000000f;
      fn->d = 0.091286342118f;
      fn->g = 2.222222222222f;
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
    case ColorSpace::TransferID::IEC61966_2_1:
    case ColorSpace::TransferID::IEC61966_2_1_HDR:
      fn->a = 0.947867345704f;
      fn->b = 0.052132654296f;
      fn->c = 0.077399380805f;
      fn->d = 0.040449937172f;
      fn->g = 2.400000000000f;
      return true;
    case ColorSpace::TransferID::BT709_APPLE:
      fn->g = 1.961000000000f;
      return true;
    case ColorSpace::TransferID::SMPTEST428_1:
      fn->a = 1.034080527699f;  // (52.37 / 48.0) ^ (1.0 / 2.6) per ITU-T H.273.
      fn->g = 2.600000000000f;
      return true;
    case ColorSpace::TransferID::IEC61966_2_4:
      // This could potentially be represented the same as IEC61966_2_1, but
      // it handles negative values differently.
      break;
    case ColorSpace::TransferID::ARIB_STD_B67:
    case ColorSpace::TransferID::BT1361_ECG:
    case ColorSpace::TransferID::LOG:
    case ColorSpace::TransferID::LOG_SQRT:
    case ColorSpace::TransferID::SMPTEST2084:
    case ColorSpace::TransferID::CUSTOM:
    case ColorSpace::TransferID::CUSTOM_HDR:
    case ColorSpace::TransferID::PIECEWISE_HDR:
    case ColorSpace::TransferID::INVALID:
      break;
  }

  return false;
}

bool ColorSpace::GetTransferFunction(skcms_TransferFunction* fn) const {
  if (transfer_ == TransferID::CUSTOM || transfer_ == TransferID::CUSTOM_HDR) {
    fn->a = transfer_params_[0];
    fn->b = transfer_params_[1];
    fn->c = transfer_params_[2];
    fn->d = transfer_params_[3];
    fn->e = transfer_params_[4];
    fn->f = transfer_params_[5];
    fn->g = transfer_params_[6];
    return true;
  } else {
    return GetTransferFunction(transfer_, fn);
  }
}

bool ColorSpace::GetInverseTransferFunction(skcms_TransferFunction* fn) const {
  if (!GetTransferFunction(fn))
    return false;
  *fn = SkTransferFnInverse(*fn);
  return true;
}

bool ColorSpace::GetPQSDRWhiteLevel(float* sdr_white_level) const {
  if (transfer_ != TransferID::SMPTEST2084)
    return false;
  if (transfer_params_[0] == 0.0f)
    *sdr_white_level = kDefaultSDRWhiteLevel;
  else
    *sdr_white_level = transfer_params_[0];
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

void ColorSpace::GetTransferMatrix(SkMatrix44* matrix) const {
  float Kr = 0;
  float Kb = 0;
  switch (matrix_) {
    case ColorSpace::MatrixID::RGB:
    case ColorSpace::MatrixID::INVALID:
      matrix->setIdentity();
      return;

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
      float data[16] = {
           0.25f, 0.5f,  0.25f, 0.0f,  // Y
          -0.25f, 0.5f, -0.25f, 0.5f,  // Cg
            0.5f, 0.0f,  -0.5f, 0.5f,  // Co
            0.0f, 0.0f,   0.0f, 1.0f
      };
      matrix->setRowMajorf(data);
      return;
    }

    // BT2020_CL is a special case.
    // Basically we return a matrix that transforms RYB values
    // to YUV values. (Note that the green component have been replaced
    // with the luminance.)
    case ColorSpace::MatrixID::BT2020_CL: {
      Kr = 0.2627f;
      Kb = 0.0593f;
      float data[16] = {1.0f, 0.0f,           0.0f, 0.0f,  // R
                        Kr,   1.0f - Kr - Kb, Kb,   0.0f,  // Y
                        0.0f, 0.0f,           1.0f, 0.0f,  // B
                        0.0f, 0.0f,           0.0f, 1.0f};
      matrix->setRowMajorf(data);
      return;
    }

    case ColorSpace::MatrixID::BT2020_NCL:
      Kr = 0.2627f;
      Kb = 0.0593f;
      break;

    case ColorSpace::MatrixID::YDZDX: {
      float data[16] = {
          0.0f,              1.0f,             0.0f, 0.0f,  // Y
          0.0f,             -0.5f, 0.986566f / 2.0f, 0.5f,  // DX or DZ
          0.5f, -0.991902f / 2.0f,             0.0f, 0.5f,  // DZ or DX
          0.0f,              0.0f,             0.0f, 1.0f,
      };
      matrix->setRowMajorf(data);
      return;
    }
    case ColorSpace::MatrixID::GBR: {
      float data[16] = {0.0f, 1.0f, 0.0f, 0.0f,  // G
                        0.0f, 0.0f, 1.0f, 0.0f,  // B
                        1.0f, 0.0f, 0.0f, 0.0f,  // R
                        0.0f, 0.0f, 0.0f, 1.0f};
      matrix->setRowMajorf(data);
      return;
    }
  }
  float Kg = 1.0f - Kr - Kb;
  float u_m = 0.5f / (1.0f - Kb);
  float v_m = 0.5f / (1.0f - Kr);
  float data[16] = {
                     Kr,        Kg,                Kb, 0.0f,  // Y
              u_m * -Kr, u_m * -Kg, u_m * (1.0f - Kb), 0.5f,  // U
      v_m * (1.0f - Kr), v_m * -Kg,         v_m * -Kb, 0.5f,  // V
                   0.0f,      0.0f,              0.0f, 1.0f,
  };
  matrix->setRowMajorf(data);
}

void ColorSpace::GetRangeAdjustMatrix(int bit_depth, SkMatrix44* matrix) const {
  DCHECK_GE(bit_depth, 8);
  switch (range_) {
    case RangeID::FULL:
    case RangeID::INVALID:
      matrix->setIdentity();
      return;

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
    case MatrixID::YCOCG: {
      matrix->setScale(scale_y, scale_y, scale_y);
      matrix->postTranslate(-16.0f / 219.0f, -16.0f / 219.0f, -16.0f / 219.0f);
      break;
    }

    case MatrixID::BT709:
    case MatrixID::FCC:
    case MatrixID::BT470BG:
    case MatrixID::SMPTE170M:
    case MatrixID::SMPTE240M:
    case MatrixID::BT2020_NCL:
    case MatrixID::BT2020_CL:
    case MatrixID::YDZDX: {
      const float a_uv = 224 << shift;
      const float scale_uv = c / a_uv;
      const float translate_uv = (a_uv - c) / (2.0f * a_uv);
      matrix->setScale(scale_y, scale_uv, scale_uv);
      matrix->postTranslate(-16.0f / 219.0f, translate_uv, translate_uv);
      break;
    }
  }
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
