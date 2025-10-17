// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_conversions.h"

#include <cmath>
#include <numeric>
#include <tuple>

#include "base/compiler_specific.h"
#include "base/numerics/angle_conversions.h"
#include "skia/ext/skcms_ext.h"
#include "skia/ext/skcolorspace_primaries.h"
#include "skia/ext/skcolorspace_trfn.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/color_space.h"

namespace gfx {

// Namespace containing some of the helper methods for color conversions.
namespace {
// https://en.wikipedia.org/wiki/CIELAB_color_space#Converting_between_CIELAB_and_CIEXYZ_coordinates
constexpr float kD50_x = 0.9642f;
constexpr float kD50_y = 1.0f;
constexpr float kD50_z = 0.8251f;

// Power function extended to all real numbers by point symmetry.
float powExt(float x, float p) {
  if (x < 0) {
    return -powf(-x, p);
  } else {
    return powf(x, p);
  }
}

skcms_Matrix3x3 getXYDZ65toXYZD50matrix() {
  constexpr float kD65_x = 0.3127f;
  constexpr float kD65_y = 0.3290f;
  skcms_Matrix3x3 adapt_d65_to_d50;
  skcms_AdaptToXYZD50(kD65_x, kD65_y, &adapt_d65_to_d50);
  return adapt_d65_to_d50;
}

const skcms_Matrix3x3 kXYZD65_to_LMS = {
    {{0.8190224432164319f, 0.3619062562801221f, -0.12887378261216414f},
     {0.0329836671980271f, 0.9292868468965546f, 0.03614466816999844f},
     {0.048177199566046255f, 0.26423952494422764f, 0.6335478258136937f}}};

const skcms_Matrix3x3 kOklab_to_LMS = {
    {{0.99999999845051981432f, 0.39633779217376785678f,
      0.21580375806075880339f},
     {1.0000000088817607767f, -0.1055613423236563494f,
      -0.063854174771705903402f},
     {1.0000000546724109177f, -0.089484182094965759684f,
      -1.2914855378640917399f}}};

typedef struct {
  std::array<float, 2> vals;
} skcms_Vector2;

float dot(const skcms_Vector2& a, const skcms_Vector2& b) {
  return a.vals[0] * b.vals[0] + a.vals[1] * b.vals[1];
}

std::tuple<float, float, float> ToTuple(const skcms::Vector3& v) {
  return std::make_tuple(v.vals[0], v.vals[1], v.vals[2]);
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

// Projects the color (l,a,b) to be within a polyhedral approximation of the
// Rec2020 gamut. This is done by finding the maximum value of alpha such that
// (l, alpha*a, alpha*b) is within that polyhedral approximation.
std::tuple<float, float, float> OklabGamutMap(float l, float a, float b) {
  // Constants for the normal vector of the plane formed by white, black, and
  // the specified vertex of the gamut.
  const skcms_Vector2 normal_R{{0.409702, -0.912219}};
  const skcms_Vector2 normal_M{{-0.397919, -0.917421}};
  const skcms_Vector2 normal_B{{-0.906800, 0.421562}};
  const skcms_Vector2 normal_C{{-0.171122, 0.985250}};
  const skcms_Vector2 normal_G{{0.460276, 0.887776}};
  const skcms_Vector2 normal_Y{{0.947925, 0.318495}};

  // For the triangles formed by white (W) or black (K) with the vertices
  // of Yellow and Red (YR), Red and Magenta (RM), etc, the constants to be
  // used to compute the intersection of a line of constant hue and luminance
  // with that plane.
  const float c0_YR = 0.091132;
  const skcms_Vector2 cW_YR{{0.070370, 0.034139}};
  const skcms_Vector2 cK_YR{{0.018170, 0.378550}};
  const float c0_RM = 0.113902;
  const skcms_Vector2 cW_RM{{0.090836, 0.036251}};
  const skcms_Vector2 cK_RM{{0.226781, 0.018764}};
  const float c0_MB = 0.161739;
  const skcms_Vector2 cW_MB{{-0.008202, -0.264819}};
  const skcms_Vector2 cK_MB{{0.187156, -0.284304}};
  const float c0_BC = 0.102047;
  const skcms_Vector2 cW_BC{{-0.014804, -0.162608}};
  const skcms_Vector2 cK_BC{{-0.276786, 0.004193}};
  const float c0_CG = 0.092029;
  const skcms_Vector2 cW_CG{{-0.038533, -0.001650}};
  const skcms_Vector2 cK_CG{{-0.232572, -0.094331}};
  const float c0_GY = 0.081709;
  const skcms_Vector2 cW_GY{{-0.034601, -0.002215}};
  const skcms_Vector2 cK_GY{{0.012185, 0.338031}};

  const float L = l;
  const float one_minus_L = 1.0 - L;
  const skcms_Vector2 ab{{a, b}};

  // Find the planes to intersect with and set the constants based on those
  // planes.
  float c0 = 0.f;
  skcms_Vector2 cW{{0.f, 0.f}};
  skcms_Vector2 cK{{0.f, 0.f}};
  if (dot(ab, normal_R) < 0.0) {
    if (dot(ab, normal_G) < 0.0) {
      if (dot(ab, normal_C) < 0.0) {
        c0 = c0_BC;
        cW = cW_BC;
        cK = cK_BC;
      } else {
        c0 = c0_CG;
        cW = cW_CG;
        cK = cK_CG;
      }
    } else {
      if (dot(ab, normal_Y) < 0.0) {
        c0 = c0_GY;
        cW = cW_GY;
        cK = cK_GY;
      } else {
        c0 = c0_YR;
        cW = cW_YR;
        cK = cK_YR;
      }
    }
  } else {
    if (dot(ab, normal_B) < 0.0) {
      if (dot(ab, normal_M) < 0.0) {
        c0 = c0_RM;
        cW = cW_RM;
        cK = cK_RM;
      } else {
        c0 = c0_MB;
        cW = cW_MB;
        cK = cK_MB;
      }
    } else {
      c0 = c0_BC;
      cW = cW_BC;
      cK = cK_BC;
    }
  }

  // Perform the intersection.
  float alpha = 1.f;

  // Intersect with the plane with white.
  const float w_denom = dot(cW, ab);
  if (w_denom > 0.f) {
    const float w_num = c0 * one_minus_L;
    if (w_num < w_denom) {
      alpha = std::min(alpha, w_num / w_denom);
    }
  }

  // Intersect with the plane with black.
  const float k_denom = dot(cK, ab);
  if (k_denom > 0.f) {
    const float k_num = c0 * L;
    if (k_num < k_denom) {
      alpha = std::min(alpha, k_num / k_denom);
    }
  }

  // Attenuate the ab coordinate by alpha.
  return std::make_tuple(L, alpha * a, alpha * b);
}

std::tuple<float, float, float> OklabToXYZD50(float l,
                                              float a,
                                              float b,
                                              bool gamut_map) {
  if (gamut_map) {
    std::tie(l, a, b) = OklabGamutMap(l, a, b);
  }
  // See definition of Oklab space at.
  // https://bottosson.github.io/posts/oklab/
  const skcms::Vector3 lab_input{{l, a, b}};
  const auto lms = skcms::Matrix3x3_apply(kOklab_to_LMS, lab_input);
  const skcms::Vector3 lms_cubed{{
      lms.vals[0] * lms.vals[0] * lms.vals[0],
      lms.vals[1] * lms.vals[1] * lms.vals[1],
      lms.vals[2] * lms.vals[2] * lms.vals[2],
  }};
  const auto xyz_d65 =
      skcms::Matrix3x3_apply_inverse(kXYZD65_to_LMS, lms_cubed);
  // Oklab's LMS matrix is defined with respect to D65 primaries, and needs
  // chromatic adaptation to D50.
  const auto xyz = skcms::Matrix3x3_apply(getXYDZ65toXYZD50matrix(), xyz_d65);
  return std::make_tuple(xyz.vals[0], xyz.vals[1], xyz.vals[2]);
}

std::tuple<float, float, float> XYZD50ToOklab(float x, float y, float z) {
  const skcms::Vector3 xyz{{x, y, z}};
  const auto xyz_d65 =
      skcms::Matrix3x3_apply_inverse(getXYDZ65toXYZD50matrix(), xyz);
  const auto lms_cubed = skcms::Matrix3x3_apply(kXYZD65_to_LMS, xyz_d65);
  const skcms::Vector3 lms{{
      powExt(lms_cubed.vals[0], 1.0f / 3.0f),
      powExt(lms_cubed.vals[1], 1.0f / 3.0f),
      powExt(lms_cubed.vals[2], 1.0f / 3.0f),
  }};
  const auto oklab = skcms::Matrix3x3_apply_inverse(kOklab_to_LMS, lms);
  return std::make_tuple(oklab.vals[0], oklab.vals[1], oklab.vals[2]);
}

std::tuple<float, float, float> LchToLab(float l, float c, float h) {
  return std::make_tuple(l, c * std::cos(base::DegToRad(h)),
                         c * std::sin(base::DegToRad(h)));
}
std::tuple<float, float, float> LabToLch(float l, float a, float b) {
  return std::make_tuple(l, std::sqrt(a * a + b * b),
                         base::RadToDeg(atan2f(b, a)));
}

std::tuple<float, float, float> SRGBToSRGBLegacy(float r, float g, float b) {
  return std::make_tuple(r * 255.0, g * 255.0, b * 255.0);
}

std::tuple<float, float, float> SRGBLegacyToSRGB(float r, float g, float b) {
  return std::make_tuple(r / 255.0, g / 255.0, b / 255.0);
}

std::tuple<float, float, float> XYZD50ToSRGB(float x, float y, float z) {
  skcms::Vector3 c{{x, y, z}};
  c = skcms::Matrix3x3_apply_inverse(SkNamedGamut::kSRGB, c);
  c = skcms::TransferFunction_apply_inverse(SkNamedTransferFn::kSRGB, c);
  return ToTuple(c);
}

std::tuple<float, float, float> SRGBToXYZD50(float r, float g, float b) {
  skcms::Vector3 c{{r, g, b}};
  c = skcms::TransferFunction_apply(SkNamedTransferFn::kSRGB, c);
  c = skcms::Matrix3x3_apply(SkNamedGamut::kSRGB, c);
  return ToTuple(c);
}

std::tuple<float, float, float> HSLToSRGB(float h, float s, float l) {
  // See https://www.w3.org/TR/css-color-4/#hsl-to-rgb
  if (!s) {
    return std::make_tuple(l, l, l);
  }

  auto f = [&h, &l, &s](float n) {
    float k = fmod(n + h / 30.0f, 12.0);
    float a = s * std::min(l, 1.0f - l);
    return l - a * std::max(-1.0f, std::min({k - 3.0f, 9.0f - k, 1.0f}));
  };

  return std::make_tuple(f(0), f(8), f(4));
}

std::tuple<float, float, float> SRGBToHSL(float r, float g, float b) {
  // See https://drafts.csswg.org/css-color-4/#rgb-to-hsl
  // TODO(crbug.com/329301908): check if there's any change after this draft
  // becomes settled.
  auto [min, max] = std::minmax({r, g, b});
  float hue = 0.0f, saturation = 0.0f, lightness = std::midpoint(min, max);
  float d = max - min;

  if (d != 0.0f) {
    saturation = (lightness == 0.0f || lightness == 1.0f)
                     ? 0.0f
                     : (max - lightness) / std::min(lightness, 1 - lightness);
    if (max == r) {
      hue = (g - b) / d + (g < b ? 6.0f : 0.0f);
    } else if (max == g) {
      hue = (b - r) / d + 2.0f;
    } else {  // if(max == b)
      hue = (r - g) / d + 4.0f;
    }
    hue *= 60.0f;
  }

  // Very out of gamut colors can produce negative saturation.
  // If so, just rotate the hue by 180 and use a positive saturation.
  // See https://github.com/w3c/csswg-drafts/issues/9222
  if (saturation < 0) {
    hue += 180;
    saturation = std::abs(saturation);
  }

  if (hue >= 360) {
    hue -= 360;
  }

  return std::make_tuple(hue, saturation, lightness);
}

std::tuple<float, float, float> HWBToSRGB(float h, float w, float b) {
  if (w + b >= 1.0f) {
    float gray = (w / (w + b));
    return std::make_tuple(gray, gray, gray);
  }

  // Leverage HSL to RGB conversion to find HWB to RGB, see
  // https://drafts.csswg.org/css-color-4/#hwb-to-rgb
  auto [red, green, blue] = HSLToSRGB(h, 1.0f, 0.5f);

  red += w - (w + b) * red;
  green += w - (w + b) * green;
  blue += w - (w + b) * blue;

  return std::make_tuple(red, green, blue);
}

static inline float SRGBToHue(float r, float g, float b) {
  // See https://drafts.csswg.org/css-color-4/#rgb-to-hwb
  // Similar to rgbToHsl, except that saturation and lightness are not
  // calculated, and potential negative saturation is ignored.
  // TODO(crbug.com/329301908): check if there's any change after this draft
  // becomes settled.
  auto [min, max] = std::minmax({r, g, b});
  float hue = 0.0f;
  float d = max - min;

  if (d != 0) {
    if (max == r) {
      hue = (g - b) / d + (g < b ? 6 : 0);
    } else if (max == g) {
      hue = (b - r) / d + 2;
    } else {
      hue = (r - g) / d + 4;
    }

    hue *= 60;
  }

  if (hue >= 360) {
    hue -= 360;
  }

  return hue;
}

std::tuple<float, float, float> SRGBToHWB(float r, float g, float b) {
  // See https://drafts.csswg.org/css-color-4/#rgb-to-hwb
  // TODO(crbug.com/329301908): check if there's any change after this draft
  // becomes settled.
  float hue = SRGBToHue(r, g, b);
  float white = std::min({r, g, b});
  float black = 1.0f - std::max({r, g, b});

  return std::make_tuple(hue, white, black);
}

SkColor4f SRGBLinearToSkColor4f(float r, float g, float b, float alpha) {
  // Several SVG rendering tests expect the inaccurate results from this
  // formulation and need to be rebaselined.
  // https://crbug.com/450045076
  skcms_TransferFunction tf_inv;
  skcms_TransferFunction_invert(&SkNamedTransferFn::kSRGB, &tf_inv);
  return SkColor4f{skcms_TransferFunction_eval(&tf_inv, r),
                   skcms_TransferFunction_eval(&tf_inv, g),
                   skcms_TransferFunction_eval(&tf_inv, b), alpha};
}

}  // namespace gfx
