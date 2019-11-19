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
    case ColorSpace::TransferID::SMPTEST2084_NON_HDR:
      // Should already be handled.
      break;

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

    case ColorSpace::TransferID::SMPTEST2084: {
      // Go from scRGB levels to 0-1.
      v *= 80.0f / 10000.0f;
      v = max(0.0f, v);
      float m1 = (2610.0f / 4096.0f) / 4.0f;
      float m2 = (2523.0f / 4096.0f) * 128.0f;
      float c1 = 3424.0f / 4096.0f;
      float c2 = (2413.0f / 4096.0f) * 32.0f;
      float c3 = (2392.0f / 4096.0f) * 32.0f;
      float p = powf(v, m1);
      return powf((c1 + c2 * p) / (1.0f + c3 * p), m2);
    }

    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    case ColorSpace::TransferID::ARIB_STD_B67: {
      const float a = 0.17883277f;
      const float b = 0.28466892f;
      const float c = 0.55991073f;
      v = max(0.0f, v);
      if (v <= 1)
        return 0.5f * sqrt(v);
      return a * log(v - b) + c;
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

    case ColorSpace::TransferID::SMPTEST2084: {
      v = max(0.0f, v);
      float m1 = (2610.0f / 4096.0f) / 4.0f;
      float m2 = (2523.0f / 4096.0f) * 128.0f;
      float c1 = 3424.0f / 4096.0f;
      float c2 = (2413.0f / 4096.0f) * 32.0f;
      float c3 = (2392.0f / 4096.0f) * 32.0f;
      float p = pow(v, 1.0f / m2);
      v = powf(max(p - c1, 0.0f) / (c2 - c3 * p), 1.0f / m1);
      // This matches the scRGB definition that 1.0 means 80 nits.
      // TODO(hubbe): It would be *nice* if 1.0 meant more than that, but
      // that might be difficult to do right now.
      v *= 10000.0f / 80.0f;
      return v;
    }

    case ColorSpace::TransferID::SMPTEST2084_NON_HDR:
      v = max(0.0f, v);
      return min(2.3f * pow(v, 2.8f), v / 5.0f + 0.8f);

    // Spec: http://www.arib.or.jp/english/html/overview/doc/2-STD-B67v1_0.pdf
    case ColorSpace::TransferID::ARIB_STD_B67: {
      v = max(0.0f, v);
      const float a = 0.17883277f;
      const float b = 0.28466892f;
      const float c = 0.55991073f;
      if (v <= 0.5f)
        return (v * 2.0f) * (v * 2.0f);
      return exp((v - c) / a) + b;
    }

    default:
      // Handled by skcms_TransferFunction.
      break;
  }
  NOTREACHED();
  return 0;
}

Transform GetTransferMatrix(const gfx::ColorSpace& color_space) {
  SkMatrix44 transfer_matrix;
  color_space.GetTransferMatrix(&transfer_matrix);
  return Transform(transfer_matrix);
}

Transform GetRangeAdjustMatrix(const gfx::ColorSpace& color_space) {
  SkMatrix44 range_adjust_matrix;
  color_space.GetRangeAdjustMatrix(&range_adjust_matrix);
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
class ColorTransformToBT2020CL;
class ColorTransformFromBT2020CL;
class ColorTransformNull;

class ColorTransformStep {
 public:
  ColorTransformStep() {}
  virtual ~ColorTransformStep() {}
  virtual ColorTransformFromLinear* GetFromLinear() { return nullptr; }
  virtual ColorTransformToBT2020CL* GetToBT2020CL() { return nullptr; }
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
  virtual bool CanAppendShaderSource() { return false; }
  // In the shader, |hdr| will appear before |src|, so any helper functions that
  // are created should be put in |hdr|. Any helper functions should have
  // |step_index| included in the function name, to ensure that there are no
  // naming conflicts.
  virtual void AppendShaderSource(std::stringstream* hdr,
                                  std::stringstream* src,
                                  size_t step_index) const {
    NOTREACHED();
  }
  virtual void AppendSkShaderSource(std::stringstream* src) const {
    NOTREACHED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ColorTransformStep);
};

class ColorTransformInternal : public ColorTransform {
 public:
  ColorTransformInternal(const ColorSpace& src,
                         const ColorSpace& dst,
                         Intent intent);
  ~ColorTransformInternal() override;

  gfx::ColorSpace GetSrcColorSpace() const override { return src_; }
  gfx::ColorSpace GetDstColorSpace() const override { return dst_; }

  void Transform(TriStim* colors, size_t num) const override {
    for (const auto& step : steps_) {
      step->Transform(colors, num);
    }
  }
  bool CanGetShaderSource() const override;
  std::string GetShaderSource() const override;
  std::string GetSkShaderSource() const override;
  bool IsIdentity() const override { return steps_.empty(); }
  size_t NumberOfStepsForTesting() const override { return steps_.size(); }

 private:
  void AppendColorSpaceToColorSpaceTransform(ColorSpace src,
                                             const ColorSpace& dst,
                                             ColorTransform::Intent intent);
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
  bool CanAppendShaderSource() override { return true; }
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

  bool CanAppendShaderSource() override { return true; }

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
  bool CanAppendShaderSource() override { return true; }
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

class ColorTransformFromLinear : public ColorTransformPerChannelTransferFn {
 public:
  // ColorTransformStep implementation.
  explicit ColorTransformFromLinear(ColorSpace::TransferID transfer)
      : ColorTransformPerChannelTransferFn(false), transfer_(transfer) {}
  ColorTransformFromLinear* GetFromLinear() override { return this; }
  bool CanAppendShaderSource() override { return true; }
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
      case ColorSpace::TransferID::SMPTEST2084:
        *src << "  v *= 80.0 / 10000.0;\n"
                "  v = max(0.0, v);\n"
             << "  " << scalar_type << " m1 = (2610.0 / 4096.0) / 4.0;\n"
             << "  " << scalar_type << " m2 = (2523.0 / 4096.0) * 128.0;\n"
             << "  " << scalar_type << " c1 = 3424.0 / 4096.0;\n"
             << "  " << scalar_type << " c2 = (2413.0 / 4096.0) * 32.0;\n"
             << "  " << scalar_type
             << " c3 = (2392.0 / 4096.0) * 32.0;\n"
                "  v =  pow((c1 + c2 * pow(v, m1)) / \n"
                "           (1.0 + c3 * pow(v, m1)), m2);\n";
        return;
      case ColorSpace::TransferID::ARIB_STD_B67:
        *src << "  " << scalar_type << " a = 0.17883277;\n"
             << "  " << scalar_type << " b = 0.28466892;\n"
             << "  " << scalar_type << " c = 0.55991073;\n"
             << "  v = max(0.0, v);\n"
                "  if (v <= 1.0)\n"
                "    v = 0.5 * sqrt(v);\n"
                "  else\n"
                "    v = a * log(v - b) + c;\n";
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
  bool CanAppendShaderSource() override { return true; }
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
      case ColorSpace::TransferID::SMPTEST2084:
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
                "  v = v2 * 10000.0 / 80.0;\n";
        return;
      case ColorSpace::TransferID::SMPTEST2084_NON_HDR:
        *src << "  v = max(0.0, v);\n"
                "  v = min(2.3 * pow(v, 2.8), v / 5.0 + 0.8);\n";
        return;
      case ColorSpace::TransferID::ARIB_STD_B67:
        *src << "  v = max(0.0, v);\n"
             << "  " << scalar_type << " a = 0.17883277;\n"
             << "  " << scalar_type << " b = 0.28466892;\n"
             << "  " << scalar_type << " c = 0.55991073;\n"
             << "  if (v <= 0.5)\n"
                "    v = (v * 2.0) * (v * 2.0);\n"
                "  else\n"
                "    v = exp((v - c) / a) + b;\n";
        return;
      default:
        break;
    }
    NOTREACHED();
  }

 private:
  ColorSpace::TransferID transfer_;
};

class ColorTransformSMPTEST2048NonHdrToLinear : public ColorTransformStep {
 public:
  // Assumes BT2020 primaries.
  static float Luma(const ColorTransform::TriStim& c) {
    return c.x() * 0.2627f + c.y() * 0.6780f + c.z() * 0.0593f;
  }
  static ColorTransform::TriStim ClipToWhite(ColorTransform::TriStim* c) {
    float maximum = max(max(c->x(), c->y()), c->z());
    if (maximum > 1.0f) {
      float l = Luma(*c);
      c->Scale(1.0f / maximum);
      ColorTransform::TriStim white(1.0f, 1.0f, 1.0f);
      white.Scale((1.0f - 1.0f / maximum) * l / Luma(white));
      ColorTransform::TriStim black(0.0f, 0.0f, 0.0f);
      *c += white - black;
    }
    return *c;
  }
  void Transform(ColorTransform::TriStim* colors, size_t num) const override {
    for (size_t i = 0; i < num; i++) {
      ColorTransform::TriStim ret(
          ToLinear(ColorSpace::TransferID::SMPTEST2084_NON_HDR, colors[i].x()),
          ToLinear(ColorSpace::TransferID::SMPTEST2084_NON_HDR, colors[i].y()),
          ToLinear(ColorSpace::TransferID::SMPTEST2084_NON_HDR, colors[i].z()));
      if (Luma(ret) > 0.0) {
        ColorTransform::TriStim smpte2084(
            ToLinear(ColorSpace::TransferID::SMPTEST2084, colors[i].x()),
            ToLinear(ColorSpace::TransferID::SMPTEST2084, colors[i].y()),
            ToLinear(ColorSpace::TransferID::SMPTEST2084, colors[i].z()));
        smpte2084.Scale(Luma(ret) / Luma(smpte2084));
        ret = ClipToWhite(&smpte2084);
      }
      colors[i] = ret;
    }
  }
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
class ColorTransformToBT2020CL : public ColorTransformStep {
 public:
  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformFromBT2020CL* next = next_untyped->GetFromBT2020CL();
    if (!next)
      return false;
    if (null_)
      return false;
    null_ = true;
    return true;
  }

  bool IsNull() override { return null_; }

  void Transform(ColorTransform::TriStim* RYB, size_t num) const override {
    for (size_t i = 0; i < num; i++) {
      float U, V;
      float B_Y = RYB[i].z() - RYB[i].y();
      if (B_Y <= 0) {
        U = B_Y / (-2.0 * -0.9702);
      } else {
        U = B_Y / (2.0 * 0.7910);
      }
      float R_Y = RYB[i].x() - RYB[i].y();
      if (R_Y <= 0) {
        V = R_Y / (-2.0 * -0.8591);
      } else {
        V = R_Y / (2.0 * 0.4969);
      }
      RYB[i] = ColorTransform::TriStim(RYB[i].y(), U + 0.5, V + 0.5);
    }
  }

 private:
  bool null_ = false;
};

// Inverse of ColorTransformToBT2020CL, see comment above for more info.
class ColorTransformFromBT2020CL : public ColorTransformStep {
 public:
  bool Join(ColorTransformStep* next_untyped) override {
    ColorTransformToBT2020CL* next = next_untyped->GetToBT2020CL();
    if (!next)
      return false;
    if (null_)
      return false;
    null_ = true;
    return true;
  }

  bool IsNull() override { return null_; }

  void Transform(ColorTransform::TriStim* YUV, size_t num) const override {
    if (null_)
      return;
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
  bool CanAppendShaderSource() override { return true; }
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

 private:
  bool null_ = false;
};

void ColorTransformInternal::AppendColorSpaceToColorSpaceTransform(
    ColorSpace src,
    const ColorSpace& dst,
    ColorTransform::Intent intent) {
  steps_.push_back(
      std::make_unique<ColorTransformMatrix>(GetRangeAdjustMatrix(src)));

  if (src.matrix_ == ColorSpace::MatrixID::BT2020_CL) {
    // BT2020 CL is a special case.
    steps_.push_back(std::make_unique<ColorTransformFromBT2020CL>());
  } else {
    steps_.push_back(
        std::make_unique<ColorTransformMatrix>(Invert(GetTransferMatrix(src))));
  }

  // If the target color space is not defined, just apply the adjust and
  // tranfer matrices. This path is used by YUV to RGB color conversion
  // when full color conversion is not enabled.
  if (!dst.IsValid())
    return;

  skcms_TransferFunction src_to_linear_fn;
  if (src.GetTransferFunction(&src_to_linear_fn)) {
    steps_.push_back(std::make_unique<ColorTransformSkTransferFn>(
        src_to_linear_fn, src.HasExtendedSkTransferFn()));
  } else if (src.transfer_ == ColorSpace::TransferID::SMPTEST2084_NON_HDR) {
    steps_.push_back(
        std::make_unique<ColorTransformSMPTEST2048NonHdrToLinear>());
  } else {
    steps_.push_back(std::make_unique<ColorTransformToLinear>(src.transfer_));
  }

  if (src.matrix_ == ColorSpace::MatrixID::BT2020_CL) {
    // BT2020 CL is a special case.
    steps_.push_back(
        std::make_unique<ColorTransformMatrix>(Invert(GetTransferMatrix(src))));
  }
  steps_.push_back(
      std::make_unique<ColorTransformMatrix>(GetPrimaryTransform(src)));

  steps_.push_back(
      std::make_unique<ColorTransformMatrix>(Invert(GetPrimaryTransform(dst))));
  if (dst.matrix_ == ColorSpace::MatrixID::BT2020_CL) {
    // BT2020 CL is a special case.
    steps_.push_back(
        std::make_unique<ColorTransformMatrix>(GetTransferMatrix(dst)));
  }

  skcms_TransferFunction dst_from_linear_fn;
  if (dst.GetInverseTransferFunction(&dst_from_linear_fn)) {
    steps_.push_back(std::make_unique<ColorTransformSkTransferFn>(
        dst_from_linear_fn, dst.HasExtendedSkTransferFn()));
  } else {
    steps_.push_back(std::make_unique<ColorTransformFromLinear>(dst.transfer_));
  }

  if (dst.matrix_ == ColorSpace::MatrixID::BT2020_CL) {
    steps_.push_back(std::make_unique<ColorTransformToBT2020CL>());
  } else {
    steps_.push_back(
        std::make_unique<ColorTransformMatrix>(GetTransferMatrix(dst)));
  }

  steps_.push_back(std::make_unique<ColorTransformMatrix>(
      Invert(GetRangeAdjustMatrix(dst))));
}

ColorTransformInternal::ColorTransformInternal(const ColorSpace& src,
                                               const ColorSpace& dst,
                                               Intent intent)
    : src_(src), dst_(dst) {
  // If no source color space is specified, do no transformation.
  // TODO(ccameron): We may want dst assume sRGB at some point in the future.
  if (!src_.IsValid())
    return;

  // SMPTEST2084_NON_HDR is not a valid destination.
  if (dst.transfer_ == ColorSpace::TransferID::SMPTEST2084_NON_HDR) {
    DLOG(ERROR) << "Invalid dst transfer function, returning identity.";
    return;
  }
  AppendColorSpaceToColorSpaceTransform(src_, dst_, intent);
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

bool ColorTransformInternal::CanGetShaderSource() const {
  for (const auto& step : steps_) {
    if (!step->CanAppendShaderSource())
      return false;
  }
  return true;
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
    const ColorSpace& dst,
    Intent intent) {
  return std::unique_ptr<ColorTransform>(
      new ColorTransformInternal(src, dst, intent));
}

ColorTransform::ColorTransform() {}
ColorTransform::~ColorTransform() {}

}  // namespace gfx
