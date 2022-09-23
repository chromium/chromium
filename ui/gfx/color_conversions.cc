// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_conversions.h"

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

}  // namespace

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

SkColor4f SRGBLinearToSkColor4f(float r, float g, float b, float alpha) {
  return SkColor4f{skcms_TransferFunction_eval(getSRGBInverseTrfn(), r),
                   skcms_TransferFunction_eval(getSRGBInverseTrfn(), g),
                   skcms_TransferFunction_eval(getSRGBInverseTrfn(), b), alpha};
}

SkColor4f XYZD50ToSkColor4f(float x, float y, float z, float alpha) {
  auto [r, g, b] = XYZD50tosRGBLinear(x, y, z);
  return SRGBLinearToSkColor4f(r, g, b, alpha);
}

SkColor4f LabToSkColor4f(float l, float a, float b, float alpha) {
  auto [x, y, z] = LabToXYZD50(l, a, b);
  return XYZD50ToSkColor4f(x, y, z, alpha);
}

}  // namespace gfx
