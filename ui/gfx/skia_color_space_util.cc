// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/skia_color_space_util.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/check.h"

namespace gfx {

float SkTransferFnEvalUnclamped(const skcms_TransferFunction& fn, float x) {
  if (x < fn.d)
    return fn.c * x + fn.f;
  return std::pow(fn.a * x + fn.b, fn.g) + fn.e;
}

float SkTransferFnEval(const skcms_TransferFunction& fn, float x) {
  float fn_at_x_unclamped = SkTransferFnEvalUnclamped(fn, x);
  return std::clamp(fn_at_x_unclamped, 0.0f, 1.0f);
}

skcms_TransferFunction SkTransferFnInverse(const skcms_TransferFunction& fn) {
  skcms_TransferFunction fn_inv = {0};
  if (fn.a > 0 && fn.g > 0) {
    double a_to_the_g = std::pow(fn.a, fn.g);
    fn_inv.a = 1.f / a_to_the_g;
    fn_inv.b = -fn.e / a_to_the_g;
    fn_inv.g = 1.f / fn.g;
  }
  fn_inv.d = fn.c * fn.d + fn.f;
  fn_inv.e = -fn.b / fn.a;
  if (fn.c != 0) {
    fn_inv.c = 1.f / fn.c;
    fn_inv.f = -fn.f / fn.c;
  }
  return fn_inv;
}

skcms_TransferFunction SkTransferFnScaled(const skcms_TransferFunction& fn,
                                          float scale) {
  if (scale == 1.f)
    return fn;
  float scale_to_g_inv = std::pow(scale, 1.f / fn.g);
  skcms_TransferFunction fn_scaled = {0};
  fn_scaled.a = fn.a * scale_to_g_inv;
  fn_scaled.b = fn.b * scale_to_g_inv;
  fn_scaled.c = fn.c * scale;
  fn_scaled.d = fn.d;
  fn_scaled.e = fn.e * scale;
  fn_scaled.f = fn.f * scale;
  fn_scaled.g = fn.g;
  return fn_scaled;
}

bool SkTransferFnsApproximatelyCancel(const skcms_TransferFunction& a,
                                      const skcms_TransferFunction& b) {
  const float kStep = 1.f / 8.f;
  const float kEpsilon = 2.5f / 256.f;
  for (float x = 0; x <= 1.f; x += kStep) {
    float a_of_x = SkTransferFnEval(a, x);
    float b_of_a_of_x = SkTransferFnEval(b, a_of_x);
    if (std::abs(b_of_a_of_x - x) > kEpsilon)
      return false;
  }
  return true;
}

bool SkTransferFnIsApproximatelyIdentity(const skcms_TransferFunction& a) {
  const float kStep = 1.f / 8.f;
  const float kEpsilon = 2.5f / 256.f;
  for (float x = 0; x <= 1.f; x += kStep) {
    float a_of_x = SkTransferFnEval(a, x);
    if (std::abs(a_of_x - x) > kEpsilon)
      return false;
  }
  return true;
}

bool SkM44IsApproximatelyIdentity(const SkM44& m) {
  const float kEpsilon = 1.f / 256.f;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      float identity_value = i == j ? 1 : 0;
      float value = m.rc(i, j);
      if (std::abs(identity_value - value) > kEpsilon)
        return false;
    }
  }
  return true;
}

SkM44 SkM44FromRowMajor3x3(const float* data) {
  DCHECK(data);
  // clang-format off
  return SkM44(data[0], data[1], data[2], 0,
               data[3], data[4], data[5], 0,
               data[6], data[7], data[8], 0,
               0, 0, 0, 1);
  // clang-format on
}

}  // namespace gfx
