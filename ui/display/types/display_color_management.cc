// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/display/types/display_color_management.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"

#include <cmath>

namespace display {

namespace {

float EvaluateLut(float x,
                  const std::vector<GammaRampRGBEntry>& lut,
                  size_t channel) {
  x = std::min(std::max(x, 0.f), 1.f);

  // If the LUT is empty, then return the identity function.
  if (lut.size() == 0) {
    return x;
  }

  // Let `i` be the floating-point index of `x`.
  const float i = x * (lut.size() - 1);

  // Let `j` be an integer and `alpha` in [0, 1]` such that
  // i = alpha * (j) + (1 - alpha) * (j + 1).
  const size_t j = static_cast<size_t>(std::floor(i));
  const float one_minus_alpha = i - j;
  const float alpha = 1.f - one_minus_alpha;

  // Interpolate the LUT entries.
  size_t j0 = j;
  size_t j1 = std::min(j + 1, lut.size() - 1);

  uint16_t lut_j0 = 0;
  uint16_t lut_j1 = 0;
  switch (channel) {
    case 0:
      lut_j0 = lut[j0].r;
      lut_j1 = lut[j1].r;
      break;
    case 1:
      lut_j0 = lut[j0].g;
      lut_j1 = lut[j1].g;
      break;
    case 2:
      lut_j0 = lut[j0].b;
      lut_j1 = lut[j1].b;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  return (alpha * lut_j0 + one_minus_alpha * lut_j1) / 65535.f;
}

}  // namespace

GammaCurve::GammaCurve() = default;
GammaCurve::GammaCurve(std::vector<GammaRampRGBEntry>&& lut)
    : lut_(std::move(lut)) {}
GammaCurve::GammaCurve(GammaCurve&& other)
    : lut_(std::move(other.lut_)), pre_curve_(std::move(other.pre_curve_)) {}
GammaCurve::GammaCurve(const GammaCurve& other) {
  *this = other;
}
GammaCurve::~GammaCurve() = default;
GammaCurve& GammaCurve::operator=(const GammaCurve& other) {
  lut_ = other.lut_;
  if (other.pre_curve_) {
    pre_curve_ = std::make_unique<GammaCurve>(*other.pre_curve_);
  }
  return *this;
}

// static
GammaCurve GammaCurve::MakeConcat(const GammaCurve& f, const GammaCurve& g) {
  if (f.IsDefaultIdentity()) {
    return g;
  }
  if (g.IsDefaultIdentity()) {
    return f;
  }
  GammaCurve result;
  result.lut_ = f.lut_;
  result.pre_curve_ = std::make_unique<GammaCurve>(g);
  return result;
}

// static
GammaCurve GammaCurve::MakeGamma(float gamma) {
  GammaCurve result;
  const size_t kSize = 1024;
  result.lut_.resize(kSize);
  for (size_t i = 0; i < kSize; ++i) {
    float x = i / 1023.f;
    float y = std::pow(x, gamma);
    uint16_t y_fixed = static_cast<uint16_t>(std::round(65535.f * y));
    result.lut_[i].r = y_fixed;
    result.lut_[i].g = y_fixed;
    result.lut_[i].b = y_fixed;
  }
  return result;
}

// static
GammaCurve GammaCurve::MakeScale(float red, float green, float blue) {
  GammaCurve result;
  const size_t kSize = 1024;
  result.lut_.resize(kSize);
  for (size_t i = 0; i < kSize; ++i) {
    float x = i / 1023.f;
    result.lut_[i].r = static_cast<uint16_t>(std::round(65535.f * red * x));
    result.lut_[i].g = static_cast<uint16_t>(std::round(65535.f * green * x));
    result.lut_[i].b = static_cast<uint16_t>(std::round(65535.f * blue * x));
  }
  return result;
}

float GammaCurve::Evaluate(float x, size_t channel) const {
  if (pre_curve_) {
    x = pre_curve_->Evaluate(x, channel);
  }
  return EvaluateLut(x, lut_, channel);
}

void GammaCurve::Evaluate(float x,
                          uint16_t& out_r,
                          uint16_t& out_g,
                          uint16_t& out_b) const {
  float r = Evaluate(x, 0);
  float g = Evaluate(x, 1);
  float b = Evaluate(x, 2);

  out_r = static_cast<uint16_t>(std::round(65535.f * r));
  out_g = static_cast<uint16_t>(std::round(65535.f * g));
  out_b = static_cast<uint16_t>(std::round(65535.f * b));
}

void GammaCurve::Evaluate(float rgb[3]) const {
  for (size_t c = 0; c < 3; ++c) {
    rgb[c] = Evaluate(rgb[c], c);
  }
}

std::string GammaCurve::ToString() const {
  std::string str = "[";
  for (size_t i = 0; i < lut_.size(); ++i) {
    str += base::StringPrintf("[%04x,%04x,%04x],", lut_[i].r, lut_[i].g,
                              lut_[i].b);
  }
  if (pre_curve_) {
    str += ",after(";
    str += pre_curve_->ToString();
    str += ")";
  }
  str += "]";
  return str;
}

std::string GammaCurve::ToActionString(const std::string& name) const {
  if (lut_.empty()) {
    return "";
  }
  std::string str = ",";
  for (size_t i = 0; i < lut_.size(); ++i) {
    str += base::StringPrintf("%s[%u]=%04x%04x%04x", name.c_str(),
                              static_cast<uint32_t>(i), lut_[i].r, lut_[i].g,
                              lut_[i].b);
    if (i != lut_.size() - 1) {
      str += ",";
    }
  }
  if (pre_curve_) {
    str += ",after(";
    str += pre_curve_->ToString();
    str += ")";
  }
  str += "";
  return str;
}

}  // namespace display
