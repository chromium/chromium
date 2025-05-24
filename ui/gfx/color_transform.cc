// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/color_transform.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <memory>
#include <sstream>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkM44.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"

using std::abs;
using std::copysign;
using std::endl;
using std::exp;
using std::log;
using std::max;
using std::min;
using std::pow;
using std::sqrt;

namespace gfx {

namespace {

// The maximum brightness of the reference display for HLG computations.
static constexpr float kHLGRefMaxLumNits = 1000.f;

// The maximum reference brightness of a PQ signal.
static constexpr float kPQRefMaxLumNits = 10000.f;

// The luminance vector in rec2020 linear space.
static constexpr float kLr = 0.2627;
static constexpr float kLg = 0.6780;
static constexpr float kLb = 0.0593;

SkM44 Invert(const SkM44& t) {
  SkM44 ret = t;
  if (!t.invert(&ret)) {
    LOG(ERROR) << "Inverse should always be possible.";
  }
  return ret;
}

float FromLinear(ColorSpace::TransferID id, float v) {
  switch (id) {
    case ColorSpace::TransferID::LOG:
      if (v < 0.01f)
        return 0.0f;
      return 1.0f + log(v) / log(10.0f) / 2.0f;

    case ColorSpace::TransferID::LOG_SQRT:
      if (v < sqrt(10.0f) / 1000.0f)
        return 0.0f;
      return 1.0f + log(v) / log(10.0f) / 2.5f;

    case ColorSpace::TransferID::IEC61966_2_4: {
      float a = 1.099296826809442f;
      float b = 0.018053968510807f;
      if (v < -b)
        return -a * pow(-v, 0.45f) + (a - 1.0f);
      else if (v <= b)
        return 4.5f * v;
      return a * pow(v, 0.45f) - (a - 1.0f);
    }

    case ColorSpace::TransferID::BT1361_ECG: {
      float a = 1.099f;
      float b = 0.018f;
      float l = 0.0045f;
      if (v < -l)
        return -(a * pow(-4.0f * v, 0.45f) + (a - 1.0f)) / 4.0f;
      else if (v <= b)
        return 4.5f * v;
      else
        return a * pow(v, 0.45f) - (a - 1.0f);
    }

    default:
      // Handled by skcms_TransferFunction.
      break;
  }
  NOTREACHED();
}

float ToLinear(ColorSpace::TransferID id, float v) {
  switch (id) {
    case ColorSpace::TransferID::LOG:
      if (v < 0.0f)
        return 0.0f;
      return pow(10.0f, (v - 1.0f) * 2.0f);

    case ColorSpace::TransferID::LOG_SQRT:
      if (v < 0.0f)
        return 0.0f;
      return pow(10.0f, (v - 1.0f) * 2.5f);

    case ColorSpace::TransferID::IEC61966_2_4: {
      float a = 1.099296826809442f;
      // Equal to FromLinear(ColorSpace::TransferID::IEC61966_2_4, -a).
      float from_linear_neg_a = -1.047844f;
      // Equal to FromLinear(ColorSpace::TransferID::IEC61966_2_4, b).
      float from_linear_b = 0.081243f;
      if (v < from_linear_neg_a)
        return -pow((a - 1.0f - v) / a, 1.0f / 0.45f);
      else if (v <= from_linear_b)
        return v / 4.5f;
      return pow((v + a - 1.0f) / a, 1.0f / 0.45f);
    }

    case ColorSpace::TransferID::BT1361_ECG: {
      float a = 1.099f;
      // Equal to FromLinear(ColorSpace::TransferID::BT1361_ECG, -l).
      float from_linear_neg_l = -0.020250f;
      // Equal to FromLinear(ColorSpace::TransferID::BT1361_ECG, b).
      float from_linear_b = 0.081000f;
      if (v < from_linear_neg_l)
        return -pow((1.0f - a - v * 4.0f) / a, 1.0f / 0.45f) / 4.0f;
      else if (v <= from_linear_b)
        return v / 4.5f;
      return pow((v + a - 1.0f) / a, 1.0f / 0.45f);
    }

    default:
      // Handled by skcms_TransferFunction.
      break;
  }
  NOTREACHED();
}

}  // namespace

ColorTransform::RuntimeOptions::RuntimeOptions() = default;
ColorTransform::RuntimeOptions::~RuntimeOptions() = default;

class ColorTransformMatrix;
class ColorTransformSkTransferFn;
class ColorTransformFromLinear;
class ColorTransformNull;

class ColorTransformStep {
 public:
  ColorTransformStep() {}

  ColorTransformStep(const ColorTransformStep&) = delete;
  ColorTransformStep& operator=(const ColorTransformStep&) = delete;

  virtual ~ColorTransformStep() {}
  virtual ColorTransformFromLinear* GetFromLinear() { return nullptr; }
  virtual ColorTransformSkTransferFn* GetSkTransferFn() { return nullptr; }
  virtual ColorTransformMatrix* GetMatrix() { return nullptr; }
  virtual ColorTransformNull* GetNull() { return nullptr; }

  // Join methods, returns true if the |next| transform was successfully
  // assimilated into |this|.
  // If Join() returns true, |next| is no longer needed and can be deleted.
  virtual bool Join(ColorTransformStep* next) { return false; }

  // Return true if this is a null transform.
  virtual bool IsNull() { return false; }
  virtual void Transform(
      ColorTransform::TriStim* color,
      size_t num,
      const ColorTransform::RuntimeOptions& options) const = 0;
};

class ColorTransformInternal : public ColorTransform {
 public:
  ColorTransformInternal(const ColorSpace& src,
                         const ColorSpace& dst,
                         const Options& options);
  ~ColorTransformInternal() override;

  gfx::ColorSpace GetSrcColorSpace() const override { return src_; }
  gfx::ColorSpace GetDstColorSpace() const override { return dst_; }

  void Transform(TriStim* colors, size_t num) const override {
    const ColorTransform::RuntimeOptions options;
    for (const auto& step : steps_) {
      step->Transform(colors, num, options);
    }
  }
  void Transform(TriStim* colors,
                 size_t num,
                 const ColorTransform::RuntimeOptions& options) const override {
    for (const auto& step : steps_) {
      step->Transform(colors, num, options);
    }
  }
  bool IsIdentity() const override { return steps_.empty(); }
  size_t NumberOfStepsForTesting() const override { return steps_.size(); }

 private:
  void AppendColorSpaceToColorSpaceTransform(const ColorSpace& src,
                                             const ColorSpace& dst,
                                             const Options& options);
  void Simplify();

  std::list<std::unique_ptr<ColorTransformStep>> steps_;
  gfx::ColorSpace src_;
  gfx::ColorSpace dst_;
};

class ColorTransformNull : public ColorTransformStep {
 public:
  ColorTransformNull* GetNull() override { return this; }
  bool IsNull() override { return true; }
  void Transform(ColorTransform::TriStim* color,
                 size_t num,
                 const ColorTransform::RuntimeOptions& options) const override {
  }
};

class ColorTransformMatrix : public ColorTransformStep {
 public:
  explicit ColorTransformMatrix(const SkM44& matrix) : matrix_(matrix) {}
  ColorTransformMatrix* GetMatrix() override { return this; }
  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformMatrix* next = next_untyped->GetMatrix();
    if (!next)
      return false;
    matrix_.postConcat(next->matrix_);
    return true;
  }

  bool IsNull() override { return SkM44IsApproximatelyIdentity(matrix_); }

  void Transform(ColorTransform::TriStim* colors,
                 size_t num,
                 const ColorTransform::RuntimeOptions& options) const override {
    for (size_t i = 0; i < num; i++) {
      auto& color = colors[i];
      SkV4 mapped = matrix_.map(color.x(), color.y(), color.z(), 1);
      color.SetPoint(mapped.x, mapped.y, mapped.z);
    }
  }

 private:
  class SkM44 matrix_;
};

class ColorTransformPerChannelTransferFn : public ColorTransformStep {
 public:
  explicit ColorTransformPerChannelTransferFn(bool extended)
      : extended_(extended) {}

  void Transform(ColorTransform::TriStim* colors,
                 size_t num,
                 const ColorTransform::RuntimeOptions& options) const override {
    for (size_t i = 0; i < num; i++) {
      ColorTransform::TriStim& c = colors[i];
      if (extended_) {
        c.set_x(copysign(Evaluate(abs(c.x())), c.x()));
        c.set_y(copysign(Evaluate(abs(c.y())), c.y()));
        c.set_z(copysign(Evaluate(abs(c.z())), c.z()));
      } else {
        c.set_x(Evaluate(c.x()));
        c.set_y(Evaluate(c.y()));
        c.set_z(Evaluate(c.z()));
      }
    }
  }

  virtual float Evaluate(float x) const = 0;

 private:
  // True if the transfer function is extended to be defined for all real
  // values by point symmetry.
  bool extended_ = false;
};

class ColorTransformSkTransferFn : public ColorTransformPerChannelTransferFn {
 public:
  explicit ColorTransformSkTransferFn(const skcms_TransferFunction& fn,
                                      bool extended)
      : ColorTransformPerChannelTransferFn(extended), fn_(fn) {}
  // ColorTransformStep implementation.
  ColorTransformSkTransferFn* GetSkTransferFn() override { return this; }
  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformSkTransferFn* next = next_untyped->GetSkTransferFn();
    if (!next)
      return false;
    if (SkTransferFnsApproximatelyCancel(fn_, next->fn_)) {
      // Set to be the identity.
      fn_.a = 1;
      fn_.b = 0;
      fn_.c = 1;
      fn_.d = 0;
      fn_.e = 0;
      fn_.f = 0;
      fn_.g = 1;
      return true;
    }
    return false;
  }
  bool IsNull() override { return SkTransferFnIsApproximatelyIdentity(fn_); }

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override {
    // Note that the sign-extension is performed by the caller.
    return SkTransferFnEvalUnclamped(fn_, v);
  }

 private:
  skcms_TransferFunction fn_;
};

// Applies the HLG OETF formulation that maps [0, 12] to [0, 1].
class ColorTransformHLG_OETF : public ColorTransformPerChannelTransferFn {
 public:
  explicit ColorTransformHLG_OETF()
      : ColorTransformPerChannelTransferFn(false) {}

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override {
    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;
    v = max(0.0f, v);
    if (v <= 1)
      return 0.5f * sqrt(v);
    return a * log(v - b) + c;
  }
};

class ColorTransformPQFromLinear : public ColorTransformPerChannelTransferFn {
 public:
  explicit ColorTransformPQFromLinear()
      : ColorTransformPerChannelTransferFn(false) {}

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override {
    v = max(0.0f, v);
    float m1 = (2610.0f / 4096.0f) / 4.0f;
    float m2 = (2523.0f / 4096.0f) * 128.0f;
    float c1 = 3424.0f / 4096.0f;
    float c2 = (2413.0f / 4096.0f) * 32.0f;
    float c3 = (2392.0f / 4096.0f) * 32.0f;
    float p = powf(v, m1);
    return powf((c1 + c2 * p) / (1.0f + c3 * p), m2);
  }
};

// Applies the HLG inverse OETF formulation that maps [0, 1] to [0, 1].
class ColorTransformHLG_InvOETF : public ColorTransformPerChannelTransferFn {
 public:
  explicit ColorTransformHLG_InvOETF()
      : ColorTransformPerChannelTransferFn(false) {}

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override {
    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    v = max(0.0f, v);
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;
    if (v <= 0.5f) {
      v = v * v * 4.0f;
    } else {
      v = exp((v - c) / a) + b;
    }
    return v / 12.f;
  }
};

class ColorTransformPQToLinear : public ColorTransformPerChannelTransferFn {
 public:
  explicit ColorTransformPQToLinear()
      : ColorTransformPerChannelTransferFn(false) {}

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override {
    v = max(0.0f, v);
    float m1 = (2610.0f / 4096.0f) / 4.0f;
    float m2 = (2523.0f / 4096.0f) * 128.0f;
    float c1 = 3424.0f / 4096.0f;
    float c2 = (2413.0f / 4096.0f) * 32.0f;
    float c3 = (2392.0f / 4096.0f) * 32.0f;
    float p = pow(v, 1.0f / m2);
    v = powf(max(p - c1, 0.0f) / (c2 - c3 * p), 1.0f / m1);
    return v;
  }
};

class ColorTransformFromLinear : public ColorTransformPerChannelTransferFn {
 public:
  // ColorTransformStep implementation.
  explicit ColorTransformFromLinear(ColorSpace::TransferID transfer)
      : ColorTransformPerChannelTransferFn(false), transfer_(transfer) {}
  ColorTransformFromLinear* GetFromLinear() override { return this; }
  bool IsNull() override { return transfer_ == ColorSpace::TransferID::LINEAR; }

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override { return FromLinear(transfer_, v); }

 private:
  friend class ColorTransformToLinear;
  ColorSpace::TransferID transfer_;
};

class ColorTransformToLinear : public ColorTransformPerChannelTransferFn {
 public:
  explicit ColorTransformToLinear(ColorSpace::TransferID transfer)
      : ColorTransformPerChannelTransferFn(false), transfer_(transfer) {}
  // ColorTransformStep implementation:
  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformFromLinear* next = next_untyped->GetFromLinear();
    if (!next)
      return false;
    if (transfer_ == next->transfer_) {
      transfer_ = ColorSpace::TransferID::LINEAR;
      return true;
    }
    return false;
  }
  bool IsNull() override { return transfer_ == ColorSpace::TransferID::LINEAR; }

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override { return ToLinear(transfer_, v); }

 private:
  ColorSpace::TransferID transfer_;
};

// Apply the HLG OOTF for a specified maximum luminance.
class ColorTransformHLG_OOTF : public ColorTransformStep {
 public:
  ColorTransformHLG_OOTF() = default;

  // ColorTransformStep implementation:
  void Transform(ColorTransform::TriStim* color,
                 size_t num,
                 const ColorTransform::RuntimeOptions& options) const override {
    const float dst_max_luminance_relative = options.dst_max_luminance_relative;
    float gamma_minus_one = 0.f;
    ComputeHLGToneMapConstants(options, gamma_minus_one);

    for (size_t i = 0; i < num; i++) {
      float L = kLr * color[i].x() + kLg * color[i].y() + kLb * color[i].z();
      if (L > 0.f) {
        color[i].Scale(powf(L, gamma_minus_one));
        // Scale the result to the full HDR range.
        color[i].Scale(dst_max_luminance_relative);
      }
    }
  }

 private:
  static void ComputeHLGToneMapConstants(
      const gfx::ColorTransform::RuntimeOptions& options,
      float& gamma_minus_one) {
    const float dst_max_luminance_nits =
        options.dst_sdr_max_luminance_nits * options.dst_max_luminance_relative;
    gamma_minus_one =
        1.2f +
        0.42f * logf(dst_max_luminance_nits / kHLGRefMaxLumNits) / logf(10.f) -
        1.f;
  }
};

// Apply the HLG OOTF for a 1,000 nit reference display.
class ColorTransformHLG_RefOOTF : public ColorTransformStep {
 public:
  ColorTransformHLG_RefOOTF() = default;

  static constexpr float kGammaMinusOne = 0.2f;

  // ColorTransformStep implementation:
  void Transform(ColorTransform::TriStim* color,
                 size_t num,
                 const ColorTransform::RuntimeOptions& options) const override {
    for (size_t i = 0; i < num; i++) {
      float L = kLr * color[i].x() + kLg * color[i].y() + kLb * color[i].z();
      if (L > 0.f) {
        color[i].Scale(powf(L, kGammaMinusOne));
      }
    }
  }
};

// Scale the color such that the luminance `input_max_value` maps to
// `output_max_value`.
class ColorTransformToneMapInRec2020Linear : public ColorTransformStep {
 public:
  explicit ColorTransformToneMapInRec2020Linear(const gfx::ColorSpace& src)
      : use_ref_max_luminance_(src.GetTransferID() ==
                               ColorSpace::TransferID::HLG) {}

  // ColorTransformStep implementation:
  void Transform(ColorTransform::TriStim* color,
                 size_t num,
                 const ColorTransform::RuntimeOptions& options) const override {
    float a = 0.f;
    float b = 0.f;
    ComputeToneMapConstants(options, a, b);

    for (size_t i = 0; i < num; i++) {
      float maximum = std::max({color[i].x(), color[i].y(), color[i].z()});
      if (maximum > 0.f) {
        color[i].Scale((1.f + a * maximum) / (1.f + b * maximum));
      }
    }
  }

 private:
  float ComputeSrcMaxLumRelative(
      const ColorTransform::RuntimeOptions& options) const {
    float src_max_lum_nits = kHLGRefMaxLumNits;
    if (!use_ref_max_luminance_) {
      const auto hdr_metadata =
          gfx::HDRMetadata::PopulateUnspecifiedWithDefaults(
              options.src_hdr_metadata);
      src_max_lum_nits = (hdr_metadata.cta_861_3 &&
                          hdr_metadata.cta_861_3->max_content_light_level > 0)
                             ? hdr_metadata.cta_861_3->max_content_light_level
                             : hdr_metadata.smpte_st_2086->luminance_max;
    }
    float sdr_white_nits = ColorSpace::kDefaultSDRWhiteLevel;
    if (options.src_hdr_metadata && options.src_hdr_metadata->ndwl) {
      sdr_white_nits = options.src_hdr_metadata->ndwl->nits;
    }
    return src_max_lum_nits / sdr_white_nits;
  }
  // Computes the constants used by the tone mapping algorithm described in
  // https://colab.research.google.com/drive/1hI10nq6L6ru_UFvz7-f7xQaQp0qarz_K
  void ComputeToneMapConstants(
      const gfx::ColorTransform::RuntimeOptions& options,
      float& a,
      float& b) const {
    const float src_max_lum_relative = ComputeSrcMaxLumRelative(options);
    if (src_max_lum_relative > options.dst_max_luminance_relative) {
      a = options.dst_max_luminance_relative /
          (src_max_lum_relative * src_max_lum_relative);
      b = 1.f / options.dst_max_luminance_relative;
    } else {
      a = 0;
      b = 0;
    }
  }

  const bool use_ref_max_luminance_;
};

// Converts from nits-relative (where 1.0 is `unity_nits` nits) to SDR-relative
// (where 1.0 is SDR white). If `use_src_sdr_white` is true then use 203 nits
// for SDR white, otherwise use `RuntimeOptions::dst_sdr_max_luminance_nits`
// for SDR white.
class ColorTransformSrcNitsToSdrRelative : public ColorTransformStep {
 public:
  ColorTransformSrcNitsToSdrRelative(float unity_nits, bool use_src_sdr_white)
      : unity_nits_(unity_nits), use_src_sdr_white_(use_src_sdr_white) {}

  // ColorTransformStep implementation:
  void Transform(ColorTransform::TriStim* color,
                 size_t num,
                 const ColorTransform::RuntimeOptions& options) const override {
    const float factor = ComputeNitsToSdrRelativeFactor(options);
    for (size_t i = 0; i < num; i++) {
      color[i].Scale(factor);
    }
  }

 private:
  float ComputeNitsToSdrRelativeFactor(
      const ColorTransform::RuntimeOptions& options) const {
    float sdr_white_nits = options.dst_sdr_max_luminance_nits;
    if (use_src_sdr_white_) {
      sdr_white_nits = ColorSpace::kDefaultSDRWhiteLevel;
      if (options.src_hdr_metadata && options.src_hdr_metadata->ndwl) {
        sdr_white_nits = options.src_hdr_metadata->ndwl->nits;
      }
    }
    return unity_nits_ / sdr_white_nits;
  }

  const float unity_nits_;
  const bool use_src_sdr_white_;
};

// Converts from SDR-relative (where 1.0 is SDR white) to nits-relative (where
// 1.0 is `unity_nits` nits). Use `RuntimeOptions::dst_sdr_max_luminance_nits`
// for the number of nits of SDR white.
class ColorTransformSdrToDstNitsRelative : public ColorTransformStep {
 public:
  explicit ColorTransformSdrToDstNitsRelative(float unity_nits)
      : unity_nits_(unity_nits) {}

  // ColorTransformStep implementation:
  void Transform(ColorTransform::TriStim* color,
                 size_t num,
                 const ColorTransform::RuntimeOptions& options) const override {
    const float factor = ComputeSdrRelativeToNitsFactor(options);
    for (size_t i = 0; i < num; i++) {
      color[i].Scale(factor);
    }
  }

 private:
  float ComputeSdrRelativeToNitsFactor(
      const ColorTransform::RuntimeOptions& options) const {
    return options.dst_sdr_max_luminance_nits / unity_nits_;
  }

  const float unity_nits_;
};

void ColorTransformInternal::AppendColorSpaceToColorSpaceTransform(
    const ColorSpace& src,
    const ColorSpace& dst,
    const Options& options) {
  // ITU-T H.273: If MatrixCoefficients is equal to 0 (Identity) or 8 (YCgCo),
  // range adjustment is performed on R,G,B samples rather than Y,U,V samples.
  const bool src_matrix_is_identity_or_ycgco =
      src.GetMatrixID() == ColorSpace::MatrixID::GBR ||
      src.GetMatrixID() == ColorSpace::MatrixID::YCOCG;
  auto src_range_adjust_matrix = std::make_unique<ColorTransformMatrix>(
      src.GetRangeAdjustMatrix(options.src_bit_depth));

  if (!src_matrix_is_identity_or_ycgco)
    steps_.push_back(std::move(src_range_adjust_matrix));

  steps_.push_back(std::make_unique<ColorTransformMatrix>(
      Invert(src.GetTransferMatrix(options.src_bit_depth))));

  if (src_matrix_is_identity_or_ycgco)
    steps_.push_back(std::move(src_range_adjust_matrix));

  // If the target color space is not defined, just apply the adjust and
  // tranfer matrices. This path is used by YUV to RGB color conversion
  // when full color conversion is not enabled.
  if (!dst.IsValid())
    return;

  switch (src.GetTransferID()) {
    case ColorSpace::TransferID::HLG:
      steps_.push_back(std::make_unique<ColorTransformHLG_InvOETF>());
      break;
    case ColorSpace::TransferID::PQ:
      steps_.push_back(std::make_unique<ColorTransformPQToLinear>());
      break;
    case ColorSpace::TransferID::SCRGB_LINEAR_80_NITS:
      steps_.push_back(std::make_unique<ColorTransformSrcNitsToSdrRelative>(
          80.f, /*use_src_sdr_white=*/false));
      break;
    default: {
      skcms_TransferFunction src_to_linear_fn;
      if (src.GetTransferFunction(&src_to_linear_fn)) {
        steps_.push_back(std::make_unique<ColorTransformSkTransferFn>(
            src_to_linear_fn, src.HasExtendedSkTransferFn()));
      } else {
        steps_.push_back(
            std::make_unique<ColorTransformToLinear>(src.GetTransferID()));
      }
    }
  }

  steps_.push_back(
      std::make_unique<ColorTransformMatrix>(src.GetPrimaryMatrix()));

  // Perform tone mapping in a linear space
  const ColorSpace rec2020_linear(
      ColorSpace::PrimaryID::BT2020, ColorSpace::TransferID::LINEAR,
      ColorSpace::MatrixID::RGB, ColorSpace::RangeID::FULL);
  switch (src.GetTransferID()) {
    case ColorSpace::TransferID::HLG: {
      // Convert from XYZ to Rec2020 primaries.
      steps_.push_back(std::make_unique<ColorTransformMatrix>(
          Invert(rec2020_linear.GetPrimaryMatrix())));

      // Apply the reference HLG OOTF.
      steps_.push_back(std::make_unique<ColorTransformHLG_RefOOTF>());

      // Convert from linear nits-relative space (where 1.0 is 1,000 nits) to
      // SDR-relative space (where 1.0 is SDR white).
      steps_.push_back(std::make_unique<ColorTransformSrcNitsToSdrRelative>(
          kHLGRefMaxLumNits, /*use_src_sdr_white=*/true));

      // If tone mapping is requested, tone map down to the available
      // headroom.
      if (options.tone_map_pq_and_hlg_to_dst) {
        steps_.push_back(
            std::make_unique<ColorTransformToneMapInRec2020Linear>(src));
      }

      // Convert back to XYZ.
      steps_.push_back(std::make_unique<ColorTransformMatrix>(
          rec2020_linear.GetPrimaryMatrix()));
      break;
    }
    case ColorSpace::TransferID::PQ: {
      // Convert from linear nits-relative space (where 1.0 is 10,000 nits) to
      // SDR-relative space (where 1.0 is SDR white).
      steps_.push_back(std::make_unique<ColorTransformSrcNitsToSdrRelative>(
          kPQRefMaxLumNits, /*use_src_sdr_white=*/true));

      if (options.tone_map_pq_and_hlg_to_dst) {
        // Convert from XYZ to Rec2020 primaries.
        steps_.push_back(std::make_unique<ColorTransformMatrix>(
            Invert(rec2020_linear.GetPrimaryMatrix())));

        // Tone map down to the available headroom.
        steps_.push_back(
            std::make_unique<ColorTransformToneMapInRec2020Linear>(src));

        // Convert back to XYZ.
        steps_.push_back(std::make_unique<ColorTransformMatrix>(
            rec2020_linear.GetPrimaryMatrix()));
      }
      break;
    }
    default:
      break;
  }

  steps_.push_back(
      std::make_unique<ColorTransformMatrix>(Invert(dst.GetPrimaryMatrix())));

  switch (dst.GetTransferID()) {
    case ColorSpace::TransferID::HLG:
      steps_.push_back(std::make_unique<ColorTransformSdrToDstNitsRelative>(
          gfx::ColorSpace::kDefaultSDRWhiteLevel));
      steps_.push_back(std::make_unique<ColorTransformHLG_OETF>());
      break;
    case ColorSpace::TransferID::PQ:
      steps_.push_back(std::make_unique<ColorTransformSdrToDstNitsRelative>(
          kPQRefMaxLumNits));
      steps_.push_back(std::make_unique<ColorTransformPQFromLinear>());
      break;
    case ColorSpace::TransferID::SCRGB_LINEAR_80_NITS:
      steps_.push_back(
          std::make_unique<ColorTransformSdrToDstNitsRelative>(80.f));
      break;
    default: {
      skcms_TransferFunction dst_from_linear_fn;
      if (dst.GetInverseTransferFunction(&dst_from_linear_fn)) {
        steps_.push_back(std::make_unique<ColorTransformSkTransferFn>(
            dst_from_linear_fn, dst.HasExtendedSkTransferFn()));
      } else {
        steps_.push_back(
            std::make_unique<ColorTransformFromLinear>(dst.GetTransferID()));
      }
      break;
    }
  }

  // ITU-T H.273: If MatrixCoefficients is equal to 0 (Identity) or 8 (YCgCo),
  // range adjustment is performed on R,G,B samples rather than Y,U,V samples.
  const bool dst_matrix_is_identity_or_ycgco =
      dst.GetMatrixID() == ColorSpace::MatrixID::GBR ||
      dst.GetMatrixID() == ColorSpace::MatrixID::YCOCG;
  auto dst_range_adjust_matrix = std::make_unique<ColorTransformMatrix>(
      Invert(dst.GetRangeAdjustMatrix(options.dst_bit_depth)));

  if (dst_matrix_is_identity_or_ycgco)
    steps_.push_back(std::move(dst_range_adjust_matrix));

  steps_.push_back(std::make_unique<ColorTransformMatrix>(
      dst.GetTransferMatrix(options.dst_bit_depth)));

  if (!dst_matrix_is_identity_or_ycgco)
    steps_.push_back(std::move(dst_range_adjust_matrix));
}

ColorTransformInternal::ColorTransformInternal(const ColorSpace& src,
                                               const ColorSpace& dst,
                                               const Options& options)
    : src_(src), dst_(dst) {
  // If no source color space is specified, do no transformation.
  // TODO(ccameron): We may want dst assume sRGB at some point in the future.
  if (!src_.IsValid())
    return;
  AppendColorSpaceToColorSpaceTransform(src_, dst_, options);
  if (!options.disable_optimizations)
    Simplify();
}

ColorTransformInternal::~ColorTransformInternal() = default;

void ColorTransformInternal::Simplify() {
  for (auto iter = steps_.begin(); iter != steps_.end();) {
    std::unique_ptr<ColorTransformStep>& this_step = *iter;

    // Try to Join |next_step| into |this_step|. If successful, re-visit the
    // step before |this_step|.
    auto iter_next = iter;
    iter_next++;
    if (iter_next != steps_.end()) {
      std::unique_ptr<ColorTransformStep>& next_step = *iter_next;
      if (this_step->Join(next_step.get())) {
        steps_.erase(iter_next);
        if (iter != steps_.begin())
          --iter;
        continue;
      }
    }

    // If |this_step| step is a no-op, remove it, and re-visit the step before
    // |this_step|.
    if (this_step->IsNull()) {
      iter = steps_.erase(iter);
      if (iter != steps_.begin())
        --iter;
      continue;
    }

    ++iter;
  }
}

// static
std::unique_ptr<ColorTransform> ColorTransform::NewColorTransform(
    const ColorSpace& src,
    const ColorSpace& dst) {
  Options options;
  return std::make_unique<ColorTransformInternal>(src, dst, options);
}

// static
std::unique_ptr<ColorTransform> ColorTransform::NewColorTransform(
    const ColorSpace& src,
    const ColorSpace& dst,
    const Options& options) {
  return std::make_unique<ColorTransformInternal>(src, dst, options);
}

ColorTransform::ColorTransform() = default;
ColorTransform::~ColorTransform() = default;

}  // namespace gfx
