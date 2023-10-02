// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_conversions.h"

#include <cmath>

#include "skia/ext/skcolorspace_primaries.h"
#include "skia/ext/skcolorspace_trfn.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace gfx {

// Namespace containing some of the helper methods for color conversions.
namespace {
// https://en.wikipedia.org/wiki/CIELAB_color_space#Converting_between_CIELAB_and_CIEXYZ_coordinates
constexpr float kD50_x = 0.9642f;
constexpr float kD50_y = 1.0f;
constexpr float kD50_z = 0.8251f;

const skcms_Matrix3x3* getXYDZ65toXYZD50matrix() {
  constexpr float kD65_x = 0.3127f;
  constexpr float kD65_y = 0.3290f;
  static skcms_Matrix3x3 adapt_d65_to_d50;
  skcms_AdaptToXYZD50(kD65_x, kD65_y, &adapt_d65_to_d50);
  return &adapt_d65_to_d50;
}

const skcms_Matrix3x3* getXYDZ50toXYZD65matrix() {
  static skcms_Matrix3x3 adapt_d50_to_d65;
  skcms_Matrix3x3_invert(getXYDZ65toXYZD50matrix(), &adapt_d50_to_d65);
  return &adapt_d50_to_d65;
}

const skcms_Matrix3x3* getXYZD50TosRGBLinearMatrix() {
  static skcms_Matrix3x3 xyzd50_to_srgb_linear;
  skcms_Matrix3x3_invert(&SkNamedGamut::kSRGB, &xyzd50_to_srgb_linear);
  return &xyzd50_to_srgb_linear;
}

const skcms_Matrix3x3* getkXYZD65tosRGBMatrix() {
  static skcms_Matrix3x3 adapt_XYZD65_to_srgb = skcms_Matrix3x3_concat(
      getXYZD50TosRGBLinearMatrix(), getXYDZ65toXYZD50matrix());
  return &adapt_XYZD65_to_srgb;
}

const skcms_Matrix3x3* getProPhotoRGBtoXYZD50Matrix() {
  static skcms_Matrix3x3 lin_proPhoto_to_XYZ_D50;
  SkNamedPrimariesExt::kProPhotoRGB.toXYZD50(&lin_proPhoto_to_XYZ_D50);
  return &lin_proPhoto_to_XYZ_D50;
}

const skcms_Matrix3x3* getXYZD50toProPhotoRGBMatrix() {
  static skcms_Matrix3x3 xyzd50_to_ProPhotoRGB;
  skcms_Matrix3x3_invert(getProPhotoRGBtoXYZD50Matrix(),
                         &xyzd50_to_ProPhotoRGB);
  return &xyzd50_to_ProPhotoRGB;
}

const skcms_Matrix3x3* getXYZD50toDisplayP3Matrix() {
  static skcms_Matrix3x3 xyzd50_to_DisplayP3;
  skcms_Matrix3x3_invert(&SkNamedGamut::kDisplayP3, &xyzd50_to_DisplayP3);
  return &xyzd50_to_DisplayP3;
}

const skcms_Matrix3x3* getXYZD50toAdobeRGBMatrix() {
  static skcms_Matrix3x3 xyzd50_to_kAdobeRGB;
  skcms_Matrix3x3_invert(&SkNamedGamut::kAdobeRGB, &xyzd50_to_kAdobeRGB);
  return &xyzd50_to_kAdobeRGB;
}

const skcms_Matrix3x3* getXYZD50toRec2020Matrix() {
  static skcms_Matrix3x3 xyzd50_to_Rec2020;
  skcms_Matrix3x3_invert(&SkNamedGamut::kRec2020, &xyzd50_to_Rec2020);
  return &xyzd50_to_Rec2020;
}

const skcms_Matrix3x3* getXYZToLMSMatrix() {
  static const skcms_Matrix3x3 kXYZ_to_LMS = {
      {{0.8190224432164319f, 0.3619062562801221f, -0.12887378261216414f},
       {0.0329836671980271f, 0.9292868468965546f, 0.03614466816999844f},
       {0.048177199566046255f, 0.26423952494422764f, 0.6335478258136937f}}};
  return &kXYZ_to_LMS;
}

const skcms_Matrix3x3* getLMSToXYZMatrix() {
  static skcms_Matrix3x3 LMS_to_XYZ;
  skcms_Matrix3x3_invert(getXYZToLMSMatrix(), &LMS_to_XYZ);
  return &LMS_to_XYZ;
}

const skcms_Matrix3x3* getOklabToLMSMatrix() {
  static const skcms_Matrix3x3 kOklab_to_LMS = {
      {{0.99999999845051981432f, 0.39633779217376785678f,
        0.21580375806075880339f},
       {1.0000000088817607767f, -0.1055613423236563494f,
        -0.063854174771705903402f},
       {1.0000000546724109177f, -0.089484182094965759684f,
        -1.2914855378640917399f}}};
  return &kOklab_to_LMS;
}

const skcms_Matrix3x3* getLMSToOklabMatrix() {
  static skcms_Matrix3x3 LMS_to_Oklab;
  skcms_Matrix3x3_invert(getOklabToLMSMatrix(), &LMS_to_Oklab);
  return &LMS_to_Oklab;
}

typedef struct {
  float vals[3];
} skcms_Vector3;

static skcms_Vector3 skcms_Matrix3x3_apply(const skcms_Matrix3x3* m,
                                           const skcms_Vector3* v) {
  skcms_Vector3 dst = {{0, 0, 0}};
  for (int row = 0; row < 3; ++row) {
    dst.vals[row] = m->vals[row][0] * v->vals[0] +
                    m->vals[row][1] * v->vals[1] + m->vals[row][2] * v->vals[2];
  }
  return dst;
}

skcms_TransferFunction* getSRGBInverseTransferFunction() {
  static skcms_TransferFunction srgb_inverse;
  skcms_TransferFunction_invert(&SkNamedTransferFn::kSRGB, &srgb_inverse);
  return &srgb_inverse;
}

std::tuple<float, float, float> ApplyInverseTransferFnsRGB(float r,
                                                           float g,
                                                           float b) {
  return std::make_tuple(
      skcms_TransferFunction_eval(getSRGBInverseTransferFunction(), r),
      skcms_TransferFunction_eval(getSRGBInverseTransferFunction(), g),
      skcms_TransferFunction_eval(getSRGBInverseTransferFunction(), b));
}

std::tuple<float, float, float> ApplyTransferFnsRGB(float r, float g, float b) {
  return std::make_tuple(
      skcms_TransferFunction_eval(&SkNamedTransferFn::kSRGB, r),
      skcms_TransferFunction_eval(&SkNamedTransferFn::kSRGB, g),
      skcms_TransferFunction_eval(&SkNamedTransferFn::kSRGB, b));
}

std::tuple<float, float, float> ApplyTransferFnProPhoto(float r,
                                                        float g,
                                                        float b) {
  return std::make_tuple(
      skcms_TransferFunction_eval(&SkNamedTransferFnExt::kProPhotoRGB, r),
      skcms_TransferFunction_eval(&SkNamedTransferFnExt::kProPhotoRGB, g),
      skcms_TransferFunction_eval(&SkNamedTransferFnExt::kProPhotoRGB, b));
}

std::tuple<float, float, float> ApplyTransferFnAdobeRGB(float r,
                                                        float g,
                                                        float b) {
  return std::make_tuple(
      skcms_TransferFunction_eval(&SkNamedTransferFn::k2Dot2, r),
      skcms_TransferFunction_eval(&SkNamedTransferFn::k2Dot2, g),
      skcms_TransferFunction_eval(&SkNamedTransferFn::k2Dot2, b));
}

skcms_TransferFunction* getProPhotoInverseTransferFunction() {
  static skcms_TransferFunction ProPhoto_inverse;
  skcms_TransferFunction_invert(&SkNamedTransferFnExt::kProPhotoRGB,
                                &ProPhoto_inverse);
  return &ProPhoto_inverse;
}

std::tuple<float, float, float> ApplyInverseTransferFnProPhoto(float r,
                                                               float g,
                                                               float b) {
  return std::make_tuple(
      skcms_TransferFunction_eval(getProPhotoInverseTransferFunction(), r),
      skcms_TransferFunction_eval(getProPhotoInverseTransferFunction(), g),
      skcms_TransferFunction_eval(getProPhotoInverseTransferFunction(), b));
}

skcms_TransferFunction* getAdobeRGBInverseTransferFunction() {
  static skcms_TransferFunction AdobeRGB_inverse;
  skcms_TransferFunction_invert(&SkNamedTransferFn::k2Dot2, &AdobeRGB_inverse);
  return &AdobeRGB_inverse;
}

std::tuple<float, float, float> ApplyInverseTransferFnAdobeRGB(float r,
                                                               float g,
                                                               float b) {
  return std::make_tuple(
      skcms_TransferFunction_eval(getAdobeRGBInverseTransferFunction(), r),
      skcms_TransferFunction_eval(getAdobeRGBInverseTransferFunction(), g),
      skcms_TransferFunction_eval(getAdobeRGBInverseTransferFunction(), b));
}

std::tuple<float, float, float> ApplyTransferFnRec2020(float r,
                                                       float g,
                                                       float b) {
  return std::make_tuple(
      skcms_TransferFunction_eval(&SkNamedTransferFn::kRec2020, r),
      skcms_TransferFunction_eval(&SkNamedTransferFn::kRec2020, g),
      skcms_TransferFunction_eval(&SkNamedTransferFn::kRec2020, b));
}

skcms_TransferFunction* getRec2020nverseTransferFunction() {
  static skcms_TransferFunction Rec2020_inverse;
  skcms_TransferFunction_invert(&SkNamedTransferFn::kRec2020, &Rec2020_inverse);
  return &Rec2020_inverse;
}

std::tuple<float, float, float> ApplyInverseTransferFnRec2020(float r,
                                                              float g,
                                                              float b) {
  return std::make_tuple(
      skcms_TransferFunction_eval(getRec2020nverseTransferFunction(), r),
      skcms_TransferFunction_eval(getRec2020nverseTransferFunction(), g),
      skcms_TransferFunction_eval(getRec2020nverseTransferFunction(), b));
}
}  // namespace

std::tuple<float, float, float> LabToXYZD50(float l, float a, float b) {
  float y = (l + 16.0f) / 116.0f;
  float x = y + a / 500.0f;
  float z = y - b / 200.0f;

  auto LabInverseTransferFunction = [](float t) {
    constexpr float delta = (24.0f / 116.0f);

    if (t <= delta) {
      return (108.0f / 841.0f) * (t - (16.0f / 116.0f));
    }

    return t * t * t;
  };

  x = LabInverseTransferFunction(x) * kD50_x;
  y = LabInverseTransferFunction(y) * kD50_y;
  z = LabInverseTransferFunction(z) * kD50_z;

  return std::make_tuple(x, y, z);
}

std::tuple<float, float, float> XYZD50ToLab(float x, float y, float z) {
  auto LabTransferFunction = [](float t) {
    constexpr float delta_limit =
        (24.0f / 116.0f) * (24.0f / 116.0f) * (24.0f / 116.0f);

    if (t <= delta_limit)
      return (841.0f / 108.0f) * t + (16.0f / 116.0f);
    else
      return std::pow(t, 1.0f / 3.0f);
  };

  x = LabTransferFunction(x / kD50_x);
  y = LabTransferFunction(y / kD50_y);
  z = LabTransferFunction(z / kD50_z);

  float l = 116.0f * y - 16.0f;
  float a = 500.0f * (x - y);
  float b = 200.0f * (y - z);

  return std::make_tuple(l, a, b);
}

std::tuple<float, float, float> OklabToXYZD65(float l, float a, float b) {
  skcms_Vector3 lab_input{{l / 100.f, a, b}};
  skcms_Vector3 lms_intermediate =
      skcms_Matrix3x3_apply(getOklabToLMSMatrix(), &lab_input);
  lms_intermediate.vals[0] = lms_intermediate.vals[0] *
                             lms_intermediate.vals[0] *
                             lms_intermediate.vals[0];
  lms_intermediate.vals[1] = lms_intermediate.vals[1] *
                             lms_intermediate.vals[1] *
                             lms_intermediate.vals[1];
  lms_intermediate.vals[2] = lms_intermediate.vals[2] *
                             lms_intermediate.vals[2] *
                             lms_intermediate.vals[2];
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(getLMSToXYZMatrix(), &lms_intermediate);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> XYZD65ToOklab(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 lms_intermediate =
      skcms_Matrix3x3_apply(getXYZToLMSMatrix(), &xyz_input);

  lms_intermediate.vals[0] = pow(lms_intermediate.vals[0], 1.0f / 3.0f);
  lms_intermediate.vals[1] = pow(lms_intermediate.vals[1], 1.0f / 3.0f);
  lms_intermediate.vals[2] = pow(lms_intermediate.vals[2], 1.0f / 3.0f);

  skcms_Vector3 lab_output =
      skcms_Matrix3x3_apply(getLMSToOklabMatrix(), &lms_intermediate);
  return std::make_tuple(lab_output.vals[0] * 100.0f, lab_output.vals[1],
                         lab_output.vals[2]);
}

std::tuple<float, float, float> LchToLab(float l, float c, float h) {
  return std::make_tuple(l, c * std::cos(gfx::DegToRad(h)),
                         c * std::sin(gfx::DegToRad(h)));
}
std::tuple<float, float, float> LabToLch(float l, float a, float b) {
  return std::make_tuple(l, std::sqrt(a * a + b * b),
                         gfx::RadToDeg(atan2f(b, a)));
}

std::tuple<float, float, float> DisplayP3ToXYZD50(float r, float g, float b) {
  auto [r_, g_, b_] = ApplyTransferFnsRGB(r, g, b);
  skcms_Vector3 rgb_input{{r_, g_, b_}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(&SkNamedGamut::kDisplayP3, &rgb_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> XYZD50ToDisplayP3(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 rgb_output =
      skcms_Matrix3x3_apply(getXYZD50toDisplayP3Matrix(), &xyz_input);
  return ApplyInverseTransferFnsRGB(rgb_output.vals[0], rgb_output.vals[1],
                                    rgb_output.vals[2]);
}

std::tuple<float, float, float> ProPhotoToXYZD50(float r, float g, float b) {
  auto [r_, g_, b_] = ApplyTransferFnProPhoto(r, g, b);
  skcms_Vector3 rgb_input{{r_, g_, b_}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(getProPhotoRGBtoXYZD50Matrix(), &rgb_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> XYZD50ToProPhoto(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 rgb_output =
      skcms_Matrix3x3_apply(getXYZD50toProPhotoRGBMatrix(), &xyz_input);
  return ApplyInverseTransferFnProPhoto(rgb_output.vals[0], rgb_output.vals[1],
                                        rgb_output.vals[2]);
}

std::tuple<float, float, float> AdobeRGBToXYZD50(float r, float g, float b) {
  auto [r_, g_, b_] = ApplyTransferFnAdobeRGB(r, g, b);
  skcms_Vector3 rgb_input{{r_, g_, b_}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(&SkNamedGamut::kAdobeRGB, &rgb_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> XYZD50ToAdobeRGB(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 rgb_output =
      skcms_Matrix3x3_apply(getXYZD50toAdobeRGBMatrix(), &xyz_input);
  return ApplyInverseTransferFnAdobeRGB(rgb_output.vals[0], rgb_output.vals[1],
                                        rgb_output.vals[2]);
}

std::tuple<float, float, float> Rec2020ToXYZD50(float r, float g, float b) {
  auto [r_, g_, b_] = ApplyTransferFnRec2020(r, g, b);
  skcms_Vector3 rgb_input{{r_, g_, b_}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(&SkNamedGamut::kRec2020, &rgb_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> XYZD50ToRec2020(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 rgb_output =
      skcms_Matrix3x3_apply(getXYZD50toRec2020Matrix(), &xyz_input);
  return ApplyInverseTransferFnRec2020(rgb_output.vals[0], rgb_output.vals[1],
                                       rgb_output.vals[2]);
}

std::tuple<float, float, float> XYZD50ToD65(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(getXYDZ50toXYZD65matrix(), &xyz_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> XYZD65ToD50(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(getXYDZ65toXYZD50matrix(), &xyz_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> XYZD65TosRGBLinear(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 rgb_result =
      skcms_Matrix3x3_apply(getkXYZD65tosRGBMatrix(), &xyz_input);
  return std::make_tuple(rgb_result.vals[0], rgb_result.vals[1],
                         rgb_result.vals[2]);
}

std::tuple<float, float, float> XYZD50TosRGBLinear(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 rgb_result =
      skcms_Matrix3x3_apply(getXYZD50TosRGBLinearMatrix(), &xyz_input);
  return std::make_tuple(rgb_result.vals[0], rgb_result.vals[1],
                         rgb_result.vals[2]);
}

std::tuple<float, float, float> SRGBLinearToXYZD50(float r, float g, float b) {
  skcms_Vector3 rgb_input{{r, g, b}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(&SkNamedGamut::kSRGB, &rgb_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> SRGBToXYZD50(float r, float g, float b) {
  auto [r_, g_, b_] = ApplyTransferFnsRGB(r, g, b);
  skcms_Vector3 rgb_input{{r_, g_, b_}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(&SkNamedGamut::kSRGB, &rgb_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> SRGBToHSL(float r, float g, float b) {
  // See https://www.w3.org/TR/css-color-4/#rgb-to-hsl
  float max = std::max({r, g, b});
  float min = std::min({r, g, b});
  float hue = 0.0f, saturation = 0.0f, ligth = (max + min) / 2.0f;
  float d = max - min;

  if (d != 0.0f) {
    saturation = (ligth == 0.0f || ligth == 1.0f)
                     ? 0.0f
                     : (max - ligth) / std::min(ligth, 1 - ligth);
    if (max == r) {
      hue = (g - b) / d + (g < b ? 6.0f : 0.0f);
    } else if (max == g) {
      hue = (b - r) / d + 2.0f;
    } else {  // if(max == b)
      hue = (r - g) / d + 4.0f;
    }
    hue *= 60.0f;
  }

  return std::make_tuple(hue, saturation, ligth);
}

std::tuple<float, float, float> SRGBToHWB(float r, float g, float b) {
  // Leverage RGB to HSL conversion to find RGB to HWB, see
  // https://www.w3.org/TR/css-color-4/#rgb-to-hwb
  auto [hue, saturation, light] = SRGBToHSL(r, g, b);
  float white = std::min({r, g, b});
  float black = 1.0f - std::max({r, g, b});

  return std::make_tuple(hue, white, black);
}

SkColor4f SRGBLinearToSkColor4f(float r, float g, float b, float alpha) {
  auto [srgb_r, srgb_g, srgb_b] = ApplyInverseTransferFnsRGB(r, g, b);
  return SkColor4f{srgb_r, srgb_g, srgb_b, alpha};
}

SkColor4f XYZD50ToSkColor4f(float x, float y, float z, float alpha) {
  auto [r, g, b] = XYZD50TosRGBLinear(x, y, z);
  return SRGBLinearToSkColor4f(r, g, b, alpha);
}

SkColor4f XYZD65ToSkColor4f(float x, float y, float z, float alpha) {
  auto [r, g, b] = XYZD65TosRGBLinear(x, y, z);
  return SRGBLinearToSkColor4f(r, g, b, alpha);
}

SkColor4f LabToSkColor4f(float l, float a, float b, float alpha) {
  auto [x, y, z] = LabToXYZD50(l, a, b);
  return XYZD50ToSkColor4f(x, y, z, alpha);
}

SkColor4f ProPhotoToSkColor4f(float r, float g, float b, float alpha) {
  auto [x, y, z] = ProPhotoToXYZD50(r, g, b);
  return XYZD50ToSkColor4f(x, y, z, alpha);
}

SkColor4f OklabToSkColor4f(float l, float a, float b, float alpha) {
  auto [x, y, z] = OklabToXYZD65(l, a, b);
  return XYZD65ToSkColor4f(x, y, z, alpha);
}

SkColor4f DisplayP3ToSkColor4f(float r, float g, float b, float alpha) {
  auto [x, y, z] = DisplayP3ToXYZD50(r, g, b);
  return XYZD50ToSkColor4f(x, y, z, alpha);
}

SkColor4f LchToSkColor4f(float l_input, float c, float h, float alpha) {
  auto [l, a, b] = LchToLab(l_input, c, h);
  auto [x, y, z] = LabToXYZD50(l, a, b);
  return XYZD50ToSkColor4f(x, y, z, alpha);
}
SkColor4f AdobeRGBToSkColor4f(float r, float g, float b, float alpha) {
  auto [x, y, z] = AdobeRGBToXYZD50(r, g, b);
  return XYZD50ToSkColor4f(x, y, z, alpha);
}

SkColor4f Rec2020ToSkColor4f(float r, float g, float b, float alpha) {
  auto [x, y, z] = Rec2020ToXYZD50(r, g, b);
  return XYZD50ToSkColor4f(x, y, z, alpha);
}

SkColor4f OklchToSkColor4f(float l_input, float c, float h, float alpha) {
  auto [l, a, b] = LchToLab(l_input, c, h);
  auto [x, y, z] = OklabToXYZD65(l, a, b);
  return XYZD65ToSkColor4f(x, y, z, alpha);
}

SkColor4f HSLToSkColor4f(float h, float s, float l, float alpha) {
  // See https://www.w3.org/TR/css-color-4/#hsl-to-rgb
  if (!s) {
    return SkColor4f{l, l, l, alpha};
  }

  auto f = [&h, &l, &s](float n) {
    float k = fmod(n + h / 30.0f, 12.0);
    float a = s * std::min(l, 1.0f - l);
    return l - a * std::max(-1.0f, std::min({k - 3.0f, 9.0f - k, 1.0f}));
  };

  return SkColor4f{f(0), f(8), f(4), alpha};
}

SkColor4f HWBToSkColor4f(float h, float w, float b, float alpha) {
  if (w + b >= 1.0f) {
    float gray = (w / (w + b));
    return SkColor4f{gray, gray, gray, alpha};
  }

  // Leverage HSL to RGB conversion to find HWB to RGB, see
  // https://drafts.csswg.org/css-color-4/#hwb-to-rgb
  SkColor4f result = HSLToSkColor4f(h, 1.0f, 0.5f, alpha);

  result.fR += w - (w + b) * result.fR;
  result.fG += w - (w + b) * result.fG;
  result.fB += w - (w + b) * result.fB;

  return result;
}
}  // namespace gfx
