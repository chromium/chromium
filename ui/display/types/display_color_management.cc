// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/types/display_color_management.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"

#include <cmath>

namespace display {

GammaCurve::GammaCurve() = default;
GammaCurve::GammaCurve(const std::vector<GammaRampRGBEntry>& lut)
    : lut_(std::move(lut)) {}
GammaCurve::GammaCurve(const GammaCurve& other) = default;
GammaCurve::GammaCurve(GammaCurve&& other) = default;
GammaCurve::~GammaCurve() = default;
GammaCurve& GammaCurve::operator=(const GammaCurve& other) = default;

void GammaCurve::Evaluate(float x,
                          uint16_t& r,
                          uint16_t& g,
                          uint16_t& b) const {
  x = std::min(std::max(x, 0.f), 1.f);

  // If the LUT is empty, then return the identity function.
  if (lut_.size() == 0) {
    r = g = b = static_cast<uint16_t>(std::round(65535.f * x));
    return;
  }

  // Let `i` be the floating-point index of `x`.
  const float i = x * (lut_.size() - 1);

  // Let `j` be an integer and `alpha` in [0, 1]` such that
  // i = alpha * (j) + (1 - alpha) * (j + 1).
  const size_t j = static_cast<size_t>(std::floor(i));
  const float one_minus_alpha = i - j;
  const float alpha = 1.f - one_minus_alpha;

  // Interpolate the LUT entries.
  size_t j0 = j;
  size_t j1 = std::min(j + 1, lut_.size() - 1);

  r = static_cast<uint16_t>(
      std::round(alpha * lut_[j0].r + one_minus_alpha * lut_[j1].r));
  g = static_cast<uint16_t>(
      std::round(alpha * lut_[j0].g + one_minus_alpha * lut_[j1].g));
  b = static_cast<uint16_t>(
      std::round(alpha * lut_[j0].b + one_minus_alpha * lut_[j1].b));
}

std::string GammaCurve::ToString() const {
  std::string str = "[";
  for (size_t i = 0; i < lut_.size(); ++i) {
    str += base::StringPrintf("[%04x,%04x,%04x],", lut_[i].r, lut_[i].g,
                              lut_[i].b);
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
  str += "";
  return str;
}

}  // namespace display
