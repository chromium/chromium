// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_transform.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <memory>
#include <sstream>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/icc_profile.h"
#include "ui/gfx/skia_color_space_util.h"
#include "ui/gfx/transform.h"

using std::abs;
using std::copysign;
using std::exp;
using std::log;
using std::max;
using std::min;
using std::pow;
using std::sqrt;
using std::endl;

namespace gfx {

namespace {

void InitStringStream(std::stringstream* ss) {
  ss->imbue(std::locale::classic());
  ss->precision(8);
  *ss << std::scientific;
}

std::string Str(float f) {
  std::stringstream ss;
  InitStringStream(&ss);
  ss << f;
  return ss.str();
}

Transform Invert(const Transform& t) {
  Transform ret = t;
  if (!t.GetInverse(&ret)) {
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
  return 0;
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
  return 0;
}

Transform GetTransferMatrix(const gfx::ColorSpace& color_space, int bit_depth) {
  SkMatrix44 transfer_matrix;
  color_space.GetTransferMatrix(bit_depth, &transfer_matrix);
  return Transform(transfer_matrix);
}

Transform GetRangeAdjustMatrix(const gfx::ColorSpace& color_space,
                               int bit_depth) {
  SkMatrix44 range_adjust_matrix;
  color_space.GetRangeAdjustMatrix(bit_depth, &range_adjust_matrix);
  return Transform(range_adjust_matrix);
}

Transform GetPrimaryTransform(const gfx::ColorSpace& color_space) {
  SkMatrix44 primary_matrix;
  color_space.GetPrimaryMatrix(&primary_matrix);
  return Transform(primary_matrix);
}

}  // namespace

class ColorTransformMatrix;
class ColorTransformSkTransferFn;
class ColorTransformFromLinear;
class ColorTransformFromBT2020CL;
class ColorTransformNull;

class ColorTransformStep {
 public:
  ColorTransformStep() {}
  virtual ~ColorTransformStep() {}
  virtual ColorTransformFromLinear* GetFromLinear() { return nullptr; }
  virtual ColorTransformFromBT2020CL* GetFromBT2020CL() { return nullptr; }
  virtual ColorTransformSkTransferFn* GetSkTransferFn() { return nullptr; }
  virtual ColorTransformMatrix* GetMatrix() { return nullptr; }
  virtual ColorTransformNull* GetNull() { return nullptr; }

  // Join methods, returns true if the |next| transform was successfully
  // assimilated into |this|.
  // If Join() returns true, |next| is no longer needed and can be deleted.
  virtual bool Join(ColorTransformStep* next) { return false; }

  // Return true if this is a null transform.
  virtual bool IsNull() { return false; }
  virtual void Transform(ColorTransform::TriStim* color, size_t num) const = 0;
  // In the shader, |hdr| will appear before |src|, so any helper functions that
  // are created should be put in |hdr|. Any helper functions should have
  // |step_index| included in the function name, to ensure that there are no
  // naming conflicts.
  virtual void AppendShaderSource(std::stringstream* hdr,
                                  std::stringstream* src,
                                  size_t step_index) const = 0;
  virtual void AppendSkShaderSource(std::stringstream* src) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ColorTransformStep);
};

class ColorTransformInternal : public ColorTransform {
 public:
  ColorTransformInternal(const ColorSpace& src,
                         int src_bit_depth,
                         const ColorSpace& dst,
                         int dst_bit_depth,
                         Intent intent);
  ~ColorTransformInternal() override;

  gfx::ColorSpace GetSrcColorSpace() const override { return src_; }
  gfx::ColorSpace GetDstColorSpace() const override { return dst_; }

  void Transform(TriStim* colors, size_t num) const override {
    for (const auto& step : steps_) {
      step->Transform(colors, num);
    }
  }
  std::string GetShaderSource() const override;
  std::string GetSkShaderSource() const override;
  bool IsIdentity() const override { return steps_.empty(); }
  size_t NumberOfStepsForTesting() const override { return steps_.size(); }

 private:
  void AppendColorSpaceToColorSpaceTransform(const ColorSpace& src,
                                             int src_bit_depth,
                                             const ColorSpace& dst,
                                             int dst_bit_depth);
  void Simplify();

  std::list<std::unique_ptr<ColorTransformStep>> steps_;
  gfx::ColorSpace src_;
  gfx::ColorSpace dst_;
};

class ColorTransformNull : public ColorTransformStep {
 public:
  ColorTransformNull* GetNull() override { return this; }
  bool IsNull() override { return true; }
  void Transform(ColorTransform::TriStim* color, size_t num) const override {}
  void AppendShaderSource(std::stringstream* hdr,
                          std::stringstream* src,
                          size_t step_index) const override {}
  void AppendSkShaderSource(std::stringstream* src) const override {}
};

class ColorTransformMatrix : public ColorTransformStep {
 public:
  explicit ColorTransformMatrix(const class Transform& matrix)
      : matrix_(matrix) {}
  ColorTransformMatrix* GetMatrix() override { return this; }
  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformMatrix* next = next_untyped->GetMatrix();
    if (!next)
      return false;
    class Transform tmp = next->matrix_;
    tmp *= matrix_;
    matrix_ = tmp;
    return true;
  }

  bool IsNull() override {
    return SkMatrixIsApproximatelyIdentity(matrix_.matrix());
  }

  void Transform(ColorTransform::TriStim* colors, size_t num) const override {
    for (size_t i = 0; i < num; i++)
      matrix_.TransformPoint(colors + i);
  }


  void AppendShaderSource(std::stringstream* hdr,
                          std::stringstream* src,
                          size_t step_index) const override {
    const SkMatrix44& m = matrix_.matrix();
    *src << "  color = mat3(";
    *src << m.get(0, 0) << ", " << m.get(1, 0) << ", " << m.get(2, 0) << ",";
    *src << endl;
    *src << "               ";
    *src << m.get(0, 1) << ", " << m.get(1, 1) << ", " << m.get(2, 1) << ",";
    *src << endl;
    *src << "               ";
    *src << m.get(0, 2) << ", " << m.get(1, 2) << ", " << m.get(2, 2) << ")";
    *src << " * color;" << endl;

    // Only print the translational component if it isn't the identity.
    if (m.get(0, 3) != 0.f || m.get(1, 3) != 0.f || m.get(2, 3) != 0.f) {
      *src << "  color += vec3(";
      *src << m.get(0, 3) << ", " << m.get(1, 3) << ", " << m.get(2, 3);
      *src << ");" << endl;
    }
  }

  void AppendSkShaderSource(std::stringstream* src) const override {
    const SkMatrix44& m = matrix_.matrix();
    *src << "  color = half4x4(";
    *src << m.get(0, 0) << ", " << m.get(1, 0) << ", " << m.get(2, 0) << ", 0,";
    *src << endl;
    *src << "               ";
    *src << m.get(0, 1) << ", " << m.get(1, 1) << ", " << m.get(2, 1) << ", 0,";
    *src << endl;
    *src << "               ";
    *src << m.get(0, 2) << ", " << m.get(1, 2) << ", " << m.get(2, 2) << ", 0,";
    *src << endl;
    *src << "0, 0, 0, 1)";
    *src << " * color;" << endl;

    // Only print the translational component if it isn't the identity.
    if (m.get(0, 3) != 0.f || m.get(1, 3) != 0.f || m.get(2, 3) != 0.f) {
      *src << "  color += half4(";
      *src << m.get(0, 3) << ", " << m.get(1, 3) << ", " << m.get(2, 3);
      *src << ", 0);" << endl;
    }
  }

 private:
  class Transform matrix_;
};

class ColorTransformPerChannelTransferFn : public ColorTransformStep {
 public:
  explicit ColorTransformPerChannelTransferFn(bool extended)
      : extended_(extended) {}

  void Transform(ColorTransform::TriStim* colors, size_t num) const override {
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

  void AppendShaderSource(std::stringstream* hdr,
                          std::stringstream* src,
                          size_t step_index) const override {
    *hdr << "float TransferFn" << step_index << "(float v) {" << endl;
    AppendTransferShaderSource(hdr, true /* is_glsl */);
    *hdr << "  return v;" << endl;
    *hdr << "}" << endl;
    if (extended_) {
      *src << "  color.r = sign(color.r) * TransferFn" << step_index
           << "(abs(color.r));" << endl;
      *src << "  color.g = sign(color.g) * TransferFn" << step_index
           << "(abs(color.g));" << endl;
      *src << "  color.b = sign(color.b) * TransferFn" << step_index
           << "(abs(color.b));" << endl;
    } else {
      *src << "  color.r = TransferFn" << step_index << "(color.r);" << endl;
      *src << "  color.g = TransferFn" << step_index << "(color.g);" << endl;
      *src << "  color.b = TransferFn" << step_index << "(color.b);" << endl;
    }
  }

  void AppendSkShaderSource(std::stringstream* src) const override {
    if (extended_) {
      *src << "{  half v = abs(color.r);" << endl;
      AppendTransferShaderSource(src, false /* is_glsl */);
      *src << "  color.r = sign(color.r) * v; }" << endl;
      *src << "{  half v = abs(color.g);" << endl;
      AppendTransferShaderSource(src, false /* is_glsl */);
      *src << "  color.g = sign(color.g) * v; }" << endl;
      *src << "{  half v = abs(color.b);" << endl;
      AppendTransferShaderSource(src, false /* is_glsl */);
      *src << "  color.b = sign(color.b) * v; }" << endl;
    } else {
      *src << "{  half v = color.r;" << endl;
      AppendTransferShaderSource(src, false /* is_glsl */);
      *src << "  color.r = v; }" << endl;
      *src << "{  half v = color.g;" << endl;
      AppendTransferShaderSource(src, false /* is_glsl */);
      *src << "  color.g = v; }" << endl;
      *src << "{  half v = color.b;" << endl;
      AppendTransferShaderSource(src, false /* is_glsl */);
      *src << "  color.b = v; }" << endl;
    }
  }

  virtual float Evaluate(float x) const = 0;
  virtual void AppendTransferShaderSource(std::stringstream* src,
                                          bool is_glsl) const = 0;

 protected:
  // True if the transfer function is extended to be defined for all real
  // values by point symmetry.
  bool extended_ = false;
};

// This class represents the piecewise-HDR function using three new parameters,
// P, Q, and R. The function is defined as:
//            0         : x < 0
//     T(x) = sRGB(x/P) : x < P
//            Q*x+R     : x >= P
// This then expands to
//            0                : x < 0
//     T(x) = C*x/P+F          : x < P*D
//            (A*x/P+B)**G + E : x < P
//            Q*x+R            : else
class ColorTransformPiecewiseHDR : public ColorTransformPerChannelTransferFn {
 public:
  static void GetParams(const gfx::ColorSpace color_space,
                        skcms_TransferFunction* fn,
                        float* p,
                        float* q,
                        float* r) {
    float sdr_joint = 1;
    float hdr_level = 1;
    color_space.GetPiecewiseHDRParams(&sdr_joint, &hdr_level);

    // P is exactly |sdr_joint|.
    *p = sdr_joint;

    if (sdr_joint < 1.f) {
      // Q and R are computed such that |sdr_joint| maps to 1 and 1) maps to
      // |hdr_level|.
      *q = (hdr_level - 1.f) / (1.f - sdr_joint);
      *r = (1.f - hdr_level * sdr_joint) / (1.f - sdr_joint);
    } else {
      // If |sdr_joint| is exactly 1, then just saturate at 1 (there is no HDR).
      *q = 0;
      *r = 1;
    }

    // Compute |fn| so that, at x, it evaluates to sRGB(x*P).
    ColorSpace::CreateSRGB().GetTransferFunction(fn);
    fn->d *= sdr_joint;
    if (sdr_joint != 0) {
      // If |sdr_joint| is 0, then we will never evaluate |fn| anyway.
      fn->a /= sdr_joint;
      fn->c /= sdr_joint;
    }
  }
  static void InvertParams(skcms_TransferFunction* fn,
                           float* p,
                           float* q,
                           float* r) {
    *fn = SkTransferFnInverse(*fn);
    float old_p = *p;
    float old_q = *q;
    float old_r = *r;
    *p = old_q * old_p + old_r;
    if (old_q != 0.f) {
      *q = 1.f / old_q;
      *r = -old_r / old_q;
    } else {
      *q = 0.f;
      *r = 1.f;
    }
  }

  ColorTransformPiecewiseHDR(const skcms_TransferFunction fn,
                             float p,
                             float q,
                             float r)
      : ColorTransformPerChannelTransferFn(false),
        fn_(fn),
        p_(p),
        q_(q),
        r_(r) {}

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override {
    if (v < 0)
      return 0;
    else if (v < fn_.d)
      return fn_.c * v + fn_.f;
    else if (v < p_)
      return std::pow(fn_.a * v + fn_.b, fn_.g) + fn_.e;
    else
      return q_ * v + r_;
  }
  void AppendTransferShaderSource(std::stringstream* result,
                                  bool is_glsl) const override {
    *result << "  if (v < 0.0) {\n";
    *result << "    v = 0.0;\n";
    *result << "  } else if (v < " << Str(fn_.d) << ") {\n";
    *result << "    v = " << Str(fn_.c) << " * v + " << Str(fn_.f) << ";"
            << endl;
    *result << "  } else if (v < " << Str(p_) << ") {\n";
    *result << "    v = pow(" << Str(fn_.a) << " * v + " << Str(fn_.b) << ", "
            << Str(fn_.g) << ") + " << Str(fn_.e) << ";\n";
    *result << "  } else {\n";
    *result << "    v = " << Str(q_) << " * v + " << Str(r_) << ";\n";
    *result << "  }\n";
  }

 private:
  // Parameters of the SDR part.
  const skcms_TransferFunction fn_;
  // The SDR joint. Below this value in the domain, the function is defined by
  // |fn_|.
  const float p_;
  // The slope of the linear HDR part.
  const float q_;
  // The intercept of the linear HDR part.
  const float r_;
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
    if (!extended_ && !next->extended_ &&
        SkTransferFnsApproximatelyCancel(fn_, next->fn_)) {
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
  void AppendTransferShaderSource(std::stringstream* result,
                                  bool is_glsl) const override {
    const float kEpsilon = 1.f / 1024.f;

    // Construct the linear segment
    //   linear = C * x + F
    // Elide operations that will be close to the identity.
    std::string linear = "v";
    if (std::abs(fn_.c - 1.f) > kEpsilon)
      linear = Str(fn_.c) + " * " + linear;
    if (std::abs(fn_.f) > kEpsilon)
      linear = linear + " + " + Str(fn_.f);

    // Construct the nonlinear segment.
    //   nonlinear = pow(A * x + B, G) + E
    // Elide operations (especially the pow) that will be close to the
    // identity.
    std::string nonlinear = "v";
    if (std::abs(fn_.a - 1.f) > kEpsilon)
      nonlinear = Str(fn_.a) + " * " + nonlinear;
    if (std::abs(fn_.b) > kEpsilon)
      nonlinear = nonlinear + " + " + Str(fn_.b);
    if (std::abs(fn_.g - 1.f) > kEpsilon)
      nonlinear = "pow(" + nonlinear + ", " + Str(fn_.g) + ")";
    if (std::abs(fn_.e) > kEpsilon)
      nonlinear = nonlinear + " + " + Str(fn_.e);

    *result << "  if (v < " << Str(fn_.d) << ")" << endl;
    *result << "    v = " << linear << ";" << endl;
    *result << "  else" << endl;
    *result << "    v = " << nonlinear << ";" << endl;
  }

 private:
  skcms_TransferFunction fn_;
};

class ColorTransformHLGFromLinear : public ColorTransformPerChannelTransferFn {
 public:
  explicit ColorTransformHLGFromLinear(float sdr_white_level)
      : ColorTransformPerChannelTransferFn(false),
        sdr_scale_factor_(sdr_white_level /
                          gfx::ColorSpace::kDefaultSDRWhiteLevel) {}

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override {
    v *= sdr_scale_factor_;

    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;
    v = max(0.0f, v);
    if (v <= 1)
      return 0.5f * sqrt(v);
    return a * log(v - b) + c;
  }

  void AppendTransferShaderSource(std::stringstream* src,
                                  bool is_glsl) const override {
    std::string scalar_type = is_glsl ? "float" : "half";
    *src << "  v = v * " << sdr_scale_factor_ << ";\n"
         << "  v = max(0.0, v);\n"
         << "  " << scalar_type << " a = 0.17883277;\n"
         << "  " << scalar_type << " b = 0.28466892;\n"
         << "  " << scalar_type << " c = 0.55991073;\n"
         << "  if (v <= 1.0)\n"
            "    v = 0.5 * sqrt(v);\n"
            "  else\n"
            "    v = a * log(v - b) + c;\n";
  }

 private:
  const float sdr_scale_factor_;
};

class ColorTransformPQFromLinear : public ColorTransformPerChannelTransferFn {
 public:
  explicit ColorTransformPQFromLinear(float sdr_white_level)
      : ColorTransformPerChannelTransferFn(false),
        sdr_white_level_(sdr_white_level) {}

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override {
    v *= sdr_white_level_ / 10000.0f;
    v = max(0.0f, v);
    float m1 = (2610.0f / 4096.0f) / 4.0f;
    float m2 = (2523.0f / 4096.0f) * 128.0f;
    float c1 = 3424.0f / 4096.0f;
    float c2 = (2413.0f / 4096.0f) * 32.0f;
    float c3 = (2392.0f / 4096.0f) * 32.0f;
    float p = powf(v, m1);
    return powf((c1 + c2 * p) / (1.0f + c3 * p), m2);
  }
  void AppendTransferShaderSource(std::stringstream* src,
                                  bool is_glsl) const override {
    std::string scalar_type = is_glsl ? "float" : "half";
    *src << "  v *= " << sdr_white_level_
         << " / 10000.0;\n"
            "  v = max(0.0, v);\n"
         << "  " << scalar_type << " m1 = (2610.0 / 4096.0) / 4.0;\n"
         << "  " << scalar_type << " m2 = (2523.0 / 4096.0) * 128.0;\n"
         << "  " << scalar_type << " c1 = 3424.0 / 4096.0;\n"
         << "  " << scalar_type << " c2 = (2413.0 / 4096.0) * 32.0;\n"
         << "  " << scalar_type
         << " c3 = (2392.0 / 4096.0) * 32.0;\n"
            "  v =  pow((c1 + c2 * pow(v, m1)) / \n"
            "           (1.0 + c3 * pow(v, m1)), m2);\n";
  }

 private:
  const float sdr_white_level_;
};

class ColorTransformHLGToLinear : public ColorTransformPerChannelTransferFn {
 public:
  explicit ColorTransformHLGToLinear(float sdr_white_level)
      : ColorTransformPerChannelTransferFn(false),
        sdr_scale_factor_(gfx::ColorSpace::kDefaultSDRWhiteLevel /
                          sdr_white_level) {}

  // ColorTransformPerChannelTransferFn implementation:
  float Evaluate(float v) const override {
    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    v = max(0.0f, v);
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;
    if (v <= 0.5f)
      v = v * v * 4.0f;
    else
      v = exp((v - c) / a) + b;
    return v * sdr_scale_factor_;
  }

  void AppendTransferShaderSource(std::stringstream* src,
                                  bool is_glsl) const override {
    std::string scalar_type = is_glsl ? "float" : "half";

    *src << "  v = max(0.0, v);\n"
         << "  " << scalar_type << " a = 0.17883277;\n"
         << "  " << scalar_type << " b = 0.28466892;\n"
         << "  " << scalar_type << " c = 0.55991073;\n"
         << "  if (v <= 0.5)\n"
            "    v = v * v * 4.0;\n"
            "  else\n"
            "    v = exp((v - c) / a) + b;\n"
            "  v = v * "
         << sdr_scale_factor_ << ";\n";
  }

 private:
  const float sdr_scale_factor_;
};

class ColorTransformPQToLinear : public ColorTransformPerChannelTransferFn {
 public:
  explicit ColorTransformPQToLinear(float sdr_white_level)
      : ColorTransformPerChannelTransferFn(false),
        sdr_white_level_(sdr_white_level) {}

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
    v *= 10000.0f / sdr_white_level_;
    return v;
  }
  void AppendTransferShaderSource(std::stringstream* src,
                                  bool is_glsl) const override {
    std::string scalar_type = is_glsl ? "float" : "half";
    *src << "  v = max(0.0, v);\n"
         << "  " << scalar_type << " m1 = (2610.0 / 4096.0) / 4.0;\n"
         << "  " << scalar_type << " m2 = (2523.0 / 4096.0) * 128.0;\n"
         << "  " << scalar_type << " c1 = 3424.0 / 4096.0;\n"
         << "  " << scalar_type << " c2 = (2413.0 / 4096.0) * 32.0;\n"
         << "  " << scalar_type << " c3 = (2392.0 / 4096.0) * 32.0;\n";
    if (is_glsl) {
      *src << "  #ifdef GL_FRAGMENT_PRECISION_HIGH\n"
              "  highp float v2 = v;\n"
              "  #else\n"
              "  float v2 = v;\n"
              "  #endif\n";
    } else {
      *src << "  " << scalar_type << " v2 = v;\n";
    }
    *src << "  v2 = pow(max(pow(v2, 1.0 / m2) - c1, 0.0) /\n"
            "              (c2 - c3 * pow(v2, 1.0 / m2)), 1.0 / m1);\n"
            "  v = v2 * 10000.0 / "
         << sdr_white_level_ << ";\n";
  }

 private:
  const float sdr_white_level_;
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
  void AppendTransferShaderSource(std::stringstream* src,
                                  bool is_glsl) const override {
    std::string scalar_type = is_glsl ? "float" : "half";
    // This is a string-ized copy-paste from FromLinear.
    switch (transfer_) {
      case ColorSpace::TransferID::LOG:
        *src << "  if (v < 0.01)\n"
                "    v = 0.0;\n"
                "  else\n"
                "    v =  1.0 + log(v) / log(10.0) / 2.0;\n";
        return;
      case ColorSpace::TransferID::LOG_SQRT:
        *src << "  if (v < sqrt(10.0) / 1000.0)\n"
                "    v = 0.0;\n"
                "  else\n"
                "    v = 1.0 + log(v) / log(10.0) / 2.5;\n";
        return;
      case ColorSpace::TransferID::IEC61966_2_4:
        *src << "  " << scalar_type << " a = 1.099296826809442;\n"
             << "  " << scalar_type << " b = 0.018053968510807;\n"
             << "  if (v < -b)\n"
                "    v = -a * pow(-v, 0.45) + (a - 1.0);\n"
                "  else if (v <= b)\n"
                "    v = 4.5 * v;\n"
                "  else\n"
                "    v = a * pow(v, 0.45) - (a - 1.0);\n";
        return;
      case ColorSpace::TransferID::BT1361_ECG:
        *src << "  " << scalar_type << " a = 1.099;\n"
             << "  " << scalar_type << " b = 0.018;\n"
             << "  " << scalar_type << " l = 0.0045;\n"
             << "  if (v < -l)\n"
                "    v = -(a * pow(-4.0 * v, 0.45) + (a - 1.0)) / 4.0;\n"
                "  else if (v <= b)\n"
                "    v = 4.5 * v;\n"
                "  else\n"
                "    v = a * pow(v, 0.45) - (a - 1.0);\n";
        return;
      default:
        break;
    }
    NOTREACHED();
  }

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

  // This is a string-ized copy-paste from ToLinear.
  void AppendTransferShaderSource(std::stringstream* src,
                                  bool is_glsl) const override {
    std::string scalar_type = is_glsl ? "float" : "half";
    switch (transfer_) {
      case ColorSpace::TransferID::LOG:
        *src << "  if (v < 0.0)\n"
                "    v = 0.0;\n"
                "  else\n"
                "    v = pow(10.0, (v - 1.0) * 2.0);\n";
        return;
      case ColorSpace::TransferID::LOG_SQRT:
        *src << "  if (v < 0.0)\n"
                "    v = 0.0;\n"
                "  else\n"
                "    v = pow(10.0, (v - 1.0) * 2.5);\n";
        return;
      case ColorSpace::TransferID::IEC61966_2_4:
        *src << "  " << scalar_type << " a = 1.099296826809442;\n"
             << "  " << scalar_type << " from_linear_neg_a = -1.047844;\n"
             << "  " << scalar_type << " from_linear_b = 0.081243;\n"
             << "  if (v < from_linear_neg_a)\n"
                "    v = -pow((a - 1.0 - v) / a, 1.0 / 0.45);\n"
                "  else if (v <= from_linear_b)\n"
                "    v = v / 4.5;\n"
                "  else\n"
                "    v = pow((v + a - 1.0) / a, 1.0 / 0.45);\n";
        return;
      case ColorSpace::TransferID::BT1361_ECG:
        *src << "  " << scalar_type << " a = 1.099;\n"
             << "  " << scalar_type << " from_linear_neg_l = -0.020250;\n"
             << "  " << scalar_type << " from_linear_b = 0.081000;\n"
             << "  if (v < from_linear_neg_l)\n"
                "    v = -pow((1.0 - a - v * 4.0) / a, 1.0 / 0.45) / 4.0;\n"
                "  else if (v <= from_linear_b)\n"
                "    v = v / 4.5;\n"
                "  else\n"
                "    v = pow((v + a - 1.0) / a, 1.0 / 0.45);\n";
        return;
      default:
        break;
    }
    NOTREACHED();
  }

 private:
  ColorSpace::TransferID transfer_;
};

// BT2020 Constant Luminance is different than most other
// ways to encode RGB values as YUV. The basic idea is that
// transfer functions are applied on the Y value instead of
// on the RGB values. However, running the transfer function
// on the U and V values doesn't make any sense since they
// are centered at 0.5. To work around this, the transfer function
// is applied to the Y, R and B values, and then the U and V
// values are calculated from that.
// In our implementation, the YUV->RGB matrix is used to
// convert YUV to RYB (the G value is replaced with an Y value.)
// Then we run the transfer function like normal, and finally
// this class is inserted as an extra step which takes calculates
// the U and V values.
class ColorTransformFromBT2020CL : public ColorTransformStep {
 public:
  void Transform(ColorTransform::TriStim* YUV, size_t num) const override {
    for (size_t i = 0; i < num; i++) {
      float Y = YUV[i].x();
      float U = YUV[i].y() - 0.5;
      float V = YUV[i].z() - 0.5;
      float B_Y, R_Y;
      if (U <= 0) {
        B_Y = U * (-2.0 * -0.9702);
      } else {
        B_Y = U * (2.0 * 0.7910);
      }
      if (V <= 0) {
        R_Y = V * (-2.0 * -0.8591);
      } else {
        R_Y = V * (2.0 * 0.4969);
      }
      // Return an RYB value, later steps will fix it.
      YUV[i] = ColorTransform::TriStim(R_Y + Y, Y, B_Y + Y);
    }
  }
  void AppendShaderSource(std::stringstream* hdr,
                          std::stringstream* src,
                          size_t step_index) const override {
    *hdr << "vec3 BT2020_YUV_to_RYB_Step" << step_index << "(vec3 color) {"
         << endl;
    *hdr << "  float Y = color.x;" << endl;
    *hdr << "  float U = color.y - 0.5;" << endl;
    *hdr << "  float V = color.z - 0.5;" << endl;
    *hdr << "  float B_Y = 0.0;" << endl;
    *hdr << "  float R_Y = 0.0;" << endl;
    *hdr << "  if (U <= 0.0) {" << endl;
    *hdr << "    B_Y = U * (-2.0 * -0.9702);" << endl;
    *hdr << "  } else {" << endl;
    *hdr << "    B_Y = U * (2.0 * 0.7910);" << endl;
    *hdr << "  }" << endl;
    *hdr << "  if (V <= 0.0) {" << endl;
    *hdr << "    R_Y = V * (-2.0 * -0.8591);" << endl;
    *hdr << "  } else {" << endl;
    *hdr << "    R_Y = V * (2.0 * 0.4969);" << endl;
    *hdr << "  }" << endl;
    *hdr << "  return vec3(R_Y + Y, Y, B_Y + Y);" << endl;
    *hdr << "}" << endl;

    *src << "  color.rgb = BT2020_YUV_to_RYB_Step" << step_index
         << "(color.rgb);" << endl;
  }

  void AppendSkShaderSource(std::stringstream* src) const override {
    NOTREACHED();
  }
};

void ColorTransformInternal::AppendColorSpaceToColorSpaceTransform(
    const ColorSpace& src,
    int src_bit_depth,
    const ColorSpace& dst,
    int dst_bit_depth) {
  // ITU-T H.273: If MatrixCoefficients is equal to 0 (Identity) or 8 (YCgCo),
  // range adjustment is performed on R,G,B samples rather than Y,U,V samples.
  const bool src_matrix_is_identity_or_ycgco =
      src.GetMatrixID() == ColorSpace::MatrixID::GBR ||
      src.GetMatrixID() == ColorSpace::MatrixID::YCOCG;
  auto src_range_adjust_matrix = std::make_unique<ColorTransformMatrix>(
      GetRangeAdjustMatrix(src, src_bit_depth));

  if (!src_matrix_is_identity_or_ycgco)
    steps_.push_back(std::move(src_range_adjust_matrix));

  if (src.GetMatrixID() == ColorSpace::MatrixID::BT2020_CL) {
    // BT2020 CL is a special case.
    steps_.push_back(std::make_unique<ColorTransformFromBT2020CL>());
  } else {
    steps_.push_back(std::make_unique<ColorTransformMatrix>(
        Invert(GetTransferMatrix(src, src_bit_depth))));
  }

  if (src_matrix_is_identity_or_ycgco)
    steps_.push_back(std::move(src_range_adjust_matrix));

  // If the target color space is not defined, just apply the adjust and
  // tranfer matrices. This path is used by YUV to RGB color conversion
  // when full color conversion is not enabled.
  if (!dst.IsValid())
    return;

  skcms_TransferFunction src_to_linear_fn;
  if (src.GetTransferFunction(&src_to_linear_fn)) {
    steps_.push_back(std::make_unique<ColorTransformSkTransferFn>(
        src_to_linear_fn, src.HasExtendedSkTransferFn()));
  } else if (src.GetTransferID() == ColorSpace::TransferID::ARIB_STD_B67) {
    float sdr_white_level = 0.f;
    src.GetSDRWhiteLevel(&sdr_white_level);
    steps_.push_back(
        std::make_unique<ColorTransformHLGToLinear>(sdr_white_level));
  } else if (src.GetTransferID() == ColorSpace::TransferID::SMPTEST2084) {
    float sdr_white_level = 0.f;
    src.GetSDRWhiteLevel(&sdr_white_level);
    steps_.push_back(
        std::make_unique<ColorTransformPQToLinear>(sdr_white_level));
  } else if (src.GetTransferID() == ColorSpace::TransferID::PIECEWISE_HDR) {
    skcms_TransferFunction fn;
    float p, q, r;
    ColorTransformPiecewiseHDR::GetParams(src, &fn, &p, &q, &r);
    steps_.push_back(std::make_unique<ColorTransformPiecewiseHDR>(fn, p, q, r));
  } else {
    steps_.push_back(
        std::make_unique<ColorTransformToLinear>(src.GetTransferID()));
  }

  if (src.GetMatrixID() == ColorSpace::MatrixID::BT2020_CL) {
    // BT2020 CL is a special case.
    steps_.push_back(std::make_unique<ColorTransformMatrix>(
        Invert(GetTransferMatrix(src, src_bit_depth))));
  }
  steps_.push_back(
      std::make_unique<ColorTransformMatrix>(GetPrimaryTransform(src)));

  steps_.push_back(
      std::make_unique<ColorTransformMatrix>(Invert(GetPrimaryTransform(dst))));
  if (dst.GetMatrixID() == ColorSpace::MatrixID::BT2020_CL) {
    // BT2020 CL is a special case.
    steps_.push_back(std::make_unique<ColorTransformMatrix>(
        GetTransferMatrix(dst, dst_bit_depth)));
  }

  skcms_TransferFunction dst_from_linear_fn;
  if (dst.GetInverseTransferFunction(&dst_from_linear_fn)) {
    steps_.push_back(std::make_unique<ColorTransformSkTransferFn>(
        dst_from_linear_fn, dst.HasExtendedSkTransferFn()));
  } else if (dst.GetTransferID() == ColorSpace::TransferID::ARIB_STD_B67) {
    float sdr_white_level = 0.f;
    dst.GetSDRWhiteLevel(&sdr_white_level);
    steps_.push_back(
        std::make_unique<ColorTransformHLGFromLinear>(sdr_white_level));
  } else if (dst.GetTransferID() == ColorSpace::TransferID::SMPTEST2084) {
    float sdr_white_level = 0.f;
    dst.GetSDRWhiteLevel(&sdr_white_level);
    steps_.push_back(
        std::make_unique<ColorTransformPQFromLinear>(sdr_white_level));
  } else if (dst.GetTransferID() == ColorSpace::TransferID::PIECEWISE_HDR) {
    skcms_TransferFunction fn;
    float p, q, r;
    ColorTransformPiecewiseHDR::GetParams(dst, &fn, &p, &q, &r);
    ColorTransformPiecewiseHDR::InvertParams(&fn, &p, &q, &r);
    steps_.push_back(std::make_unique<ColorTransformPiecewiseHDR>(fn, p, q, r));
  } else {
    steps_.push_back(
        std::make_unique<ColorTransformFromLinear>(dst.GetTransferID()));
  }

  // ITU-T H.273: If MatrixCoefficients is equal to 0 (Identity) or 8 (YCgCo),
  // range adjustment is performed on R,G,B samples rather than Y,U,V samples.
  const bool dst_matrix_is_identity_or_ycgco =
      dst.GetMatrixID() == ColorSpace::MatrixID::GBR ||
      dst.GetMatrixID() == ColorSpace::MatrixID::YCOCG;
  auto dst_range_adjust_matrix = std::make_unique<ColorTransformMatrix>(
      Invert(GetRangeAdjustMatrix(dst, dst_bit_depth)));

  if (dst_matrix_is_identity_or_ycgco)
    steps_.push_back(std::move(dst_range_adjust_matrix));

  if (dst.GetMatrixID() == ColorSpace::MatrixID::BT2020_CL) {
    NOTREACHED();
  } else {
    steps_.push_back(std::make_unique<ColorTransformMatrix>(
        GetTransferMatrix(dst, dst_bit_depth)));
  }

  if (!dst_matrix_is_identity_or_ycgco)
    steps_.push_back(std::move(dst_range_adjust_matrix));
}

ColorTransformInternal::ColorTransformInternal(const ColorSpace& src,
                                               int src_bit_depth,
                                               const ColorSpace& dst,
                                               int dst_bit_depth,
                                               Intent intent)
    : src_(src), dst_(dst) {
  // If no source color space is specified, do no transformation.
  // TODO(ccameron): We may want dst assume sRGB at some point in the future.
  if (!src_.IsValid())
    return;
  AppendColorSpaceToColorSpaceTransform(src_, src_bit_depth, dst_,
                                        dst_bit_depth);
  if (intent != Intent::TEST_NO_OPT)
    Simplify();
}

std::string ColorTransformInternal::GetShaderSource() const {
  std::stringstream hdr;
  std::stringstream src;
  InitStringStream(&hdr);
  InitStringStream(&src);
  src << "vec3 DoColorConversion(vec3 color) {" << endl;
  size_t step_index = 0;
  for (const auto& step : steps_)
    step->AppendShaderSource(&hdr, &src, step_index++);
  src << "  return color;" << endl;
  src << "}" << endl;
  return hdr.str() + src.str();
}

std::string ColorTransformInternal::GetSkShaderSource() const {
  std::stringstream src;
  InitStringStream(&src);
  for (const auto& step : steps_)
    step->AppendSkShaderSource(&src);
  return src.str();
}

ColorTransformInternal::~ColorTransformInternal() {}

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
    int src_bit_depth,
    const ColorSpace& dst,
    int dst_bit_depth,
    Intent intent) {
  return std::make_unique<ColorTransformInternal>(src, src_bit_depth, dst,
                                                  dst_bit_depth, intent);
}

ColorTransform::ColorTransform() {}
ColorTransform::~ColorTransform() {}

}  // namespace gfx
