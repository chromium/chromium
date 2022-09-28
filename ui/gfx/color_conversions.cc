// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_conversions.h"

#include "skia/ext/skcolorspace_primaries.h"
#include "skia/ext/skcolorspace_trfn.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/color_space.h"

namespace gfx {

// Namespace containing some of the helper methods for color conversions.
namespace {
skcms_Matrix3x3* getXYDZ65toXYZD50matrix() {
  constexpr float kD65_X = 0.3127f;
  constexpr float kD65_Y = 0.3290f;
  static skcms_Matrix3x3 adapt_d65_to_d50;
  skcms_AdaptToXYZD50(kD65_X, kD65_Y, &adapt_d65_to_d50);
  return &adapt_d65_to_d50;
}

skcms_Matrix3x3* getXYDZ50toXYZD65matrix() {
  static skcms_Matrix3x3 adapt_d50_to_d65;
  skcms_Matrix3x3_invert(getXYDZ65toXYZD50matrix(), &adapt_d50_to_d65);
  return &adapt_d50_to_d65;
}

skcms_Matrix3x3* getXYZD50tosSRGBLinearMatrix() {
  static skcms_Matrix3x3 xyzd50_to_srgb_linear;
  skcms_Matrix3x3_invert(&SkNamedGamut::kSRGB, &xyzd50_to_srgb_linear);
  return &xyzd50_to_srgb_linear;
}

skcms_Matrix3x3* getProPhotoRGBtoXYZD50Matrix() {
  static skcms_Matrix3x3 lin_proPhoto_to_XYZ_D50;
  SkNamedPrimariesExt::kProPhotoRGB.toXYZD50(&lin_proPhoto_to_XYZ_D50);
  return &lin_proPhoto_to_XYZ_D50;
}

skcms_Matrix3x3* getkXYZD65tosRGBMatrix() {
  static skcms_Matrix3x3 adapt_XYZD65_to_srgb = skcms_Matrix3x3_concat(
      getXYZD50tosSRGBLinearMatrix(), getXYDZ65toXYZD50matrix());
  return &adapt_XYZD65_to_srgb;
}

float LabInverseTransferFunction(float t) {
  // https://en.wikipedia.org/wiki/CIELAB_color_space#Converting_between_CIELAB_and_CIEXYZ_coordinates
  const float delta = (24.0f / 116.0f);

  if (t <= delta) {
    return (108.0f / 841.0f) * (t - (16.0f / 116.0f));
  }

  return t * t * t;
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

skcms_TransferFunction* getSRGBInverseTrfn() {
  static skcms_TransferFunction srgb_inverse;
  skcms_TransferFunction_invert(&SkNamedTransferFn::kSRGB, &srgb_inverse);
  return &srgb_inverse;
}

std::tuple<float, float, float> ApplyInverseTransferFnsRGB(float r,
                                                           float g,
                                                           float b) {
  return std::make_tuple(skcms_TransferFunction_eval(getSRGBInverseTrfn(), r),
                         skcms_TransferFunction_eval(getSRGBInverseTrfn(), g),
                         skcms_TransferFunction_eval(getSRGBInverseTrfn(), b));
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

std::tuple<float, float, float> ApplyTransferFnRec2020(float r,
                                                       float g,
                                                       float b) {
  return std::make_tuple(
      skcms_TransferFunction_eval(&SkNamedTransferFn::kRec2020, r),
      skcms_TransferFunction_eval(&SkNamedTransferFn::kRec2020, g),
      skcms_TransferFunction_eval(&SkNamedTransferFn::kRec2020, b));
}
}  // namespace

std::tuple<float, float, float> LchToLab(float l,
                                         float c,
                                         absl::optional<float> h) {
  if (!h.has_value())
    return std::make_tuple(l, 0, 0);

  return std::make_tuple(l, c * std::cos(h.value() * 3.141592f / 180.0f),
                         c * std::sin(h.value() * 3.141592f / 180.0f));
}

std::tuple<float, float, float> LabToXYZD50(float l, float a, float b) {
  // https://en.wikipedia.org/wiki/CIELAB_color_space#Converting_between_CIELAB_and_CIEXYZ_coordinates
  float y = (l + 16.0f) / 116.0f;
  float x = y + a / 500.0f;
  float z = y - b / 200.0f;
  constexpr float kD50_x = 0.9642f;
  constexpr float kD50_y = 1.0f;
  constexpr float kD50_z = 0.8249f;

  x = LabInverseTransferFunction(x) * kD50_x;
  y = LabInverseTransferFunction(y) * kD50_y;
  z = LabInverseTransferFunction(z) * kD50_z;

  return std::make_tuple(x, y, z);
}

std::tuple<float, float, float> OKLabToXYZD65(float l, float a, float b) {
  // Given OKLab, convert to XYZ relative to D65
  skcms_Matrix3x3 LMStoXYZ = {
      {{1.2268798733741557f, -0.5578149965554813f, 0.28139105017721583f},
       {-0.04057576262431372f, 1.1122868293970594f, -0.07171106666151701f},
       {-0.07637294974672142f, -0.4214933239627914f, 1.5869240244272418f}}};
  skcms_Matrix3x3 OKLabtoLMS = {
      {{0.99999999845051981432f, 0.39633779217376785678f,
        0.21580375806075880339f},
       {1.0000000088817607767f, -0.1055613423236563494f,
        -0.063854174771705903402f},
       {1.0000000546724109177f, -0.089484182094965759684f,
        -1.2914855378640917399f}}};

  skcms_Vector3 lab_input{{l / 100.f, a, b}};
  skcms_Vector3 lms_intermediate =
      skcms_Matrix3x3_apply(&OKLabtoLMS, &lab_input);
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
      skcms_Matrix3x3_apply(&LMStoXYZ, &lms_intermediate);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> XYZD50toD65(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(getXYDZ50toXYZD65matrix(), &xyz_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> XYZD65tosRGBLinear(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 rgb_result =
      skcms_Matrix3x3_apply(getkXYZD65tosRGBMatrix(), &xyz_input);
  return std::make_tuple(rgb_result.vals[0], rgb_result.vals[1],
                         rgb_result.vals[2]);
}

std::tuple<float, float, float> XYZD50tosRGBLinear(float x, float y, float z) {
  skcms_Vector3 xyz_input{{x, y, z}};
  skcms_Vector3 rgb_result =
      skcms_Matrix3x3_apply(getXYZD50tosSRGBLinearMatrix(), &xyz_input);
  return std::make_tuple(rgb_result.vals[0], rgb_result.vals[1],
                         rgb_result.vals[2]);
}

std::tuple<float, float, float> ProPhotoToXYZD50(float r, float g, float b) {
  auto [r_, g_, b_] = ApplyTransferFnProPhoto(r, g, b);
  skcms_Vector3 rgb_input{{r_, g_, b_}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(getProPhotoRGBtoXYZD50Matrix(), &rgb_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> DisplayP3ToXYZD50(float r, float g, float b) {
  auto [r_, g_, b_] = ApplyTransferFnsRGB(r, g, b);
  skcms_Vector3 rgb_input{{r_, g_, b_}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(&SkNamedGamut::kDisplayP3, &rgb_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> AdobeRGBToXYZD50(float r, float g, float b) {
  auto [r_, g_, b_] = ApplyTransferFnAdobeRGB(r, g, b);
  skcms_Vector3 rgb_input{{r_, g_, b_}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(&SkNamedGamut::kAdobeRGB, &rgb_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

std::tuple<float, float, float> Rec2020ToXYZD50(float r, float g, float b) {
  auto [r_, g_, b_] = ApplyTransferFnRec2020(r, g, b);
  skcms_Vector3 rgb_input{{r_, g_, b_}};
  skcms_Vector3 xyz_output =
      skcms_Matrix3x3_apply(&SkNamedGamut::kRec2020, &rgb_input);
  return std::make_tuple(xyz_output.vals[0], xyz_output.vals[1],
                         xyz_output.vals[2]);
}

SkColor4f SRGBLinearToSkColor4f(float r, float g, float b, float alpha) {
  auto [srgb_r, srgb_g, srgb_b] = ApplyInverseTransferFnsRGB(r, g, b);
  return SkColor4f{srgb_r, srgb_g, srgb_b, alpha};
}

SkColor4f XYZD50ToSkColor4f(float x, float y, float z, float alpha) {
  auto [r, g, b] = XYZD50tosRGBLinear(x, y, z);
  return SRGBLinearToSkColor4f(r, g, b, alpha);
}

SkColor4f XYZD65ToSkColor4f(float x, float y, float z, float alpha) {
  auto [r, g, b] = XYZD65tosRGBLinear(x, y, z);
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

SkColor4f OKLabToSkColor4f(float l, float a, float b, float alpha) {
  auto [x, y, z] = OKLabToXYZD65(l, a, b);
  return XYZD65ToSkColor4f(x, y, z, alpha);
}

SkColor4f DisplayP3ToSkColor4f(float r, float g, float b, float alpha) {
  auto [x, y, z] = DisplayP3ToXYZD50(r, g, b);
  return XYZD50ToSkColor4f(x, y, z, alpha);
}

SkColor4f LchToSkColor4f(float l_input,
                         float c,
                         absl::optional<float> h,
                         float alpha) {
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

SkColor4f OKLchToSkColor4f(float l_input,
                           float c,
                           absl::optional<float> h,
                           float alpha) {
  auto [l, a, b] = LchToLab(l_input, c, h);
  auto [x, y, z] = OKLabToXYZD65(l, a, b);
  return XYZD65ToSkColor4f(x, y, z, alpha);
}

}  // namespace gfx
