// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_utils.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <ostream>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_palette.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#include "skia/ext/skia_utils_win.h"
#endif

namespace color_utils {

namespace {

// The darkest reference color in color_utils.
SkColor g_darkest_color = gfx::kGoogleGrey900;

// The luminance midpoint for determining if a color is light or dark.  This is
// the value where white and g_darkest_color contrast equally.  This default
// value is the midpoint given kGoogleGrey900 as the darkest color.
float g_luminance_midpoint = 0.211692036f;

constexpr float kWhiteLuminance = 1.0f;

int calcHue(float temp1, float temp2, float hue) {
  if (hue < 0.0f)
    ++hue;
  else if (hue > 1.0f)
    --hue;

  float result = temp1;
  if (hue * 6.0f < 1.0f)
    result = temp1 + (temp2 - temp1) * hue * 6.0f;
  else if (hue * 2.0f < 1.0f)
    result = temp2;
  else if (hue * 3.0f < 2.0f)
    result = temp1 + (temp2 - temp1) * (2.0f / 3.0f - hue) * 6.0f;

  return base::ClampRound(result * 255);
}

// Assumes sRGB.
float Linearize(float eight_bit_component) {
  const float component = eight_bit_component / 255.0f;
  // The W3C link in the header uses 0.03928 here.  See
  // https://en.wikipedia.org/wiki/SRGB#Theory_of_the_transformation for
  // discussion of why we use this value rather than that one.
  return (component <= 0.04045f) ? (component / 12.92f)
                                 : pow((component + 0.055f) / 1.055f, 2.4f);
}

constexpr size_t kNumGoogleColors = 10;
constexpr SkColor kGrey[kNumGoogleColors] = {
    gfx::kGoogleGrey050, gfx::kGoogleGrey100, gfx::kGoogleGrey200,
    gfx::kGoogleGrey300, gfx::kGoogleGrey400, gfx::kGoogleGrey500,
    gfx::kGoogleGrey600, gfx::kGoogleGrey700, gfx::kGoogleGrey800,
    gfx::kGoogleGrey900,
};

constexpr SkColor kRed[kNumGoogleColors] = {
    gfx::kGoogleRed050, gfx::kGoogleRed100, gfx::kGoogleRed200,
    gfx::kGoogleRed300, gfx::kGoogleRed400, gfx::kGoogleRed500,
    gfx::kGoogleRed600, gfx::kGoogleRed700, gfx::kGoogleRed800,
    gfx::kGoogleRed900,
};

constexpr SkColor kOrange[kNumGoogleColors] = {
    gfx::kGoogleOrange050, gfx::kGoogleOrange100, gfx::kGoogleOrange200,
    gfx::kGoogleOrange300, gfx::kGoogleOrange400, gfx::kGoogleOrange500,
    gfx::kGoogleOrange600, gfx::kGoogleOrange700, gfx::kGoogleOrange800,
    gfx::kGoogleOrange900,
};

constexpr SkColor kYellow[kNumGoogleColors] = {
    gfx::kGoogleYellow050, gfx::kGoogleYellow100, gfx::kGoogleYellow200,
    gfx::kGoogleYellow300, gfx::kGoogleYellow400, gfx::kGoogleYellow500,
    gfx::kGoogleYellow600, gfx::kGoogleYellow700, gfx::kGoogleYellow800,
    gfx::kGoogleYellow900,
};

constexpr SkColor kGreen[kNumGoogleColors] = {
    gfx::kGoogleGreen050, gfx::kGoogleGreen100, gfx::kGoogleGreen200,
    gfx::kGoogleGreen300, gfx::kGoogleGreen400, gfx::kGoogleGreen500,
    gfx::kGoogleGreen600, gfx::kGoogleGreen700, gfx::kGoogleGreen800,
    gfx::kGoogleGreen900,
};

constexpr SkColor kCyan[kNumGoogleColors] = {
    gfx::kGoogleCyan050, gfx::kGoogleCyan100, gfx::kGoogleCyan200,
    gfx::kGoogleCyan300, gfx::kGoogleCyan400, gfx::kGoogleCyan500,
    gfx::kGoogleCyan600, gfx::kGoogleCyan700, gfx::kGoogleCyan800,
    gfx::kGoogleCyan900,
};

constexpr SkColor kBlue[kNumGoogleColors] = {
    gfx::kGoogleBlue050, gfx::kGoogleBlue100, gfx::kGoogleBlue200,
    gfx::kGoogleBlue300, gfx::kGoogleBlue400, gfx::kGoogleBlue500,
    gfx::kGoogleBlue600, gfx::kGoogleBlue700, gfx::kGoogleBlue800,
    gfx::kGoogleBlue900,
};

constexpr SkColor kPurple[kNumGoogleColors] = {
    gfx::kGooglePurple050, gfx::kGooglePurple100, gfx::kGooglePurple200,
    gfx::kGooglePurple300, gfx::kGooglePurple400, gfx::kGooglePurple500,
    gfx::kGooglePurple600, gfx::kGooglePurple700, gfx::kGooglePurple800,
    gfx::kGooglePurple900,
};

constexpr SkColor kMagenta[kNumGoogleColors] = {
    gfx::kGoogleMagenta050, gfx::kGoogleMagenta100, gfx::kGoogleMagenta200,
    gfx::kGoogleMagenta300, gfx::kGoogleMagenta400, gfx::kGoogleMagenta500,
    gfx::kGoogleMagenta600, gfx::kGoogleMagenta700, gfx::kGoogleMagenta800,
    gfx::kGoogleMagenta900,
};

constexpr SkColor kPink[kNumGoogleColors] = {
    gfx::kGooglePink050, gfx::kGooglePink100, gfx::kGooglePink200,
    gfx::kGooglePink300, gfx::kGooglePink400, gfx::kGooglePink500,
    gfx::kGooglePink600, gfx::kGooglePink700, gfx::kGooglePink800,
    gfx::kGooglePink900,
};

SkColor PickGoogleColor(const SkColor (&colors)[kNumGoogleColors],
                        SkColor background_color,
                        float target_contrast,
                        float min_contrast) {
  // Traverse from the least-contrasty color to the most.  First, start at the
  // "side" (light or dark) containing the background color.
  size_t start = 0, end = kNumGoogleColors - 1;
  int increment = 1;
  const float background_luminance = GetRelativeLuminance(background_color);
  if (background_luminance < g_luminance_midpoint) {
    std::swap(start, end);
    increment = -increment;
  }
  // Skip forward until contrast begins to increase, i.e. we're moving from the
  // background color toward the endpoint with maximum contrast.
  const auto contrast_ratio = [&](size_t index) {
    return GetContrastRatio(GetRelativeLuminance(colors[index]),
                            background_luminance);
  };
  while (contrast_ratio(start) > contrast_ratio(start + increment)) {
    start += increment;
    DCHECK_NE(start, end);  // `colors` must all lie on one side of the midpoint
  }

  // Now seek the color with the closest contrast to the target that's over the
  // minimum.
  //
  // If our target contrast ratio is e.g. 4:1, a ratio of 2:1 is "as far away"
  // as a ratio of 8:1; namely, both are a factor of 2 off.  For this reason, we
  // can't simply compare contrast ratios by subtracting them and looking for
  // the smallest difference.  Instead, compute the ratio of the two contrast
  // ratios in question (as larger/smaller, so the ratio will always be >= 1);
  // then when this ratio is closest to 1, we have the contrast ratio closest to
  // our target.
  const auto ratio_of_ratios = [&](float contrast) {
    auto [min, max] = std::minmax(contrast, target_contrast);
    return max / min;
  };
  absl::optional<size_t> best_index;
  absl::optional<float> best_ratio;
  for (size_t i = start; i != end; i += increment) {
    const float contrast = contrast_ratio(i);
    if (contrast < min_contrast)
      continue;  // Haven't yet reached min contrast
    const float ratio = ratio_of_ratios(contrast);
    if (best_ratio && ratio > *best_ratio)
      return colors[*best_index];  // Past the optimum point
    best_index = i;
    best_ratio = ratio;
  }
  return colors[(best_ratio &&
                 ratio_of_ratios(contrast_ratio(end)) > *best_ratio)
                    ? *best_index
                    : end];
}

}  // namespace

SkColor PickGoogleColor(SkColor color,
                        SkColor background_color,
                        float min_contrast) {
  const float target_contrast = GetContrastRatio(color, background_color);

  const auto pick_color = [&](const SkColor(&colors)[kNumGoogleColors]) {
    return PickGoogleColor(colors, background_color, target_contrast,
                           min_contrast);
  };

  HSL hsl;
  SkColorToHSL(color, &hsl);
  if (hsl.s < 0.1) {
    // Low saturation, let this be a grey.
    return pick_color(kGrey);
  }

  // Map hue to angles for readability.
  const float color_angle = hsl.h * 360;

  // Hues in comments below are of the corresponding kGoogleXXX500 color.
  // Every cutoff is a halfway point between the two neighboring hue values to
  // provide as fair of a representation as possible for what color should be
  // used.
  // RED: 4
  if (color_angle < 15)
    return pick_color(kRed);
  // ORANGE: 26
  if (color_angle < 35)
    return pick_color(kOrange);
  // YELLOW: 44
  if (color_angle < 90)
    return pick_color(kYellow);
  // GREEN: 136
  if (color_angle < 163)
    return pick_color(kGreen);
  // CYAN: 189
  // In dark mode, the Mac system blue hue is right on the border between a
  // kGoogleCyan and kGoogleBlue color, so the cutoff point is tweaked to make
  // it map to a kGoogleBlue color.
  if (color_angle < 202)
    return pick_color(kCyan);
  // BLUE: 217
  if (color_angle < 245)
    return pick_color(kBlue);
  // PURPLE: 272
  if (color_angle < 284)
    return pick_color(kPurple);
  // MAGENTA: 295
  if (color_angle < 311)
    return pick_color(kMagenta);
  // PINK: 326
  if (color_angle < 345)
    return pick_color(kPink);

  // End of hue wheel is red.
  return pick_color(kRed);
}

float GetContrastRatio(SkColor color_a, SkColor color_b) {
  return GetContrastRatio(GetRelativeLuminance(color_a),
                          GetRelativeLuminance(color_b));
}

float GetContrastRatio(float luminance_a, float luminance_b) {
  DCHECK_GE(luminance_a, 0.0f);
  DCHECK_GE(luminance_b, 0.0f);
  luminance_a += 0.05f;
  luminance_b += 0.05f;
  return (luminance_a > luminance_b) ? (luminance_a / luminance_b)
                                     : (luminance_b / luminance_a);
}

float GetRelativeLuminance(SkColor color) {
  return (0.2126f * Linearize(SkColorGetR(color))) +
         (0.7152f * Linearize(SkColorGetG(color))) +
         (0.0722f * Linearize(SkColorGetB(color)));
}

uint8_t GetLuma(SkColor color) {
  return base::ClampRound<uint8_t>(0.299f * SkColorGetR(color) +
                                   0.587f * SkColorGetG(color) +
                                   0.114f * SkColorGetB(color));
}

void SkColorToHSL(SkColor c, HSL* hsl) {
  float r = SkColorGetR(c) / 255.0f;
  float g = SkColorGetG(c) / 255.0f;
  float b = SkColorGetB(c) / 255.0f;
  float vmax = std::max({r, g, b});
  float vmin = std::min({r, g, b});
  float delta = vmax - vmin;
  hsl->l = (vmax + vmin) / 2;
  if (SkColorGetR(c) == SkColorGetG(c) && SkColorGetR(c) == SkColorGetB(c)) {
    hsl->h = hsl->s = 0;
  } else {
    float dr = (((vmax - r) / 6.0f) + (delta / 2.0f)) / delta;
    float dg = (((vmax - g) / 6.0f) + (delta / 2.0f)) / delta;
    float db = (((vmax - b) / 6.0f) + (delta / 2.0f)) / delta;
    // We need to compare for the max value because comparing vmax to r, g, or b
    // can sometimes result in values overflowing registers.
    if (r >= g && r >= b)
      hsl->h = db - dg;
    else if (g >= r && g >= b)
      hsl->h = (1.0f / 3.0f) + dr - db;
    else  // (b >= r && b >= g)
      hsl->h = (2.0f / 3.0f) + dg - dr;

    if (hsl->h < 0.0f)
      ++hsl->h;
    else if (hsl->h > 1.0f)
      --hsl->h;

    hsl->s = delta / ((hsl->l < 0.5f) ? (vmax + vmin) : (2 - vmax - vmin));
  }
}

SkColor HSLToSkColor(const HSL& hsl, SkAlpha alpha) {
  float hue = hsl.h;
  float saturation = hsl.s;
  float lightness = hsl.l;

  // If there's no color, we don't care about hue and can do everything based on
  // brightness.
  if (!saturation) {
    const uint8_t light = base::ClampRound<uint8_t>(lightness * 255);
    return SkColorSetARGB(alpha, light, light, light);
  }

  float temp2 = (lightness < 0.5f)
                    ? (lightness * (1.0f + saturation))
                    : (lightness + saturation - (lightness * saturation));
  float temp1 = 2.0f * lightness - temp2;
  return SkColorSetARGB(alpha, calcHue(temp1, temp2, hue + 1.0f / 3.0f),
                        calcHue(temp1, temp2, hue),
                        calcHue(temp1, temp2, hue - 1.0f / 3.0f));
}

bool IsWithinHSLRange(const HSL& hsl,
                      const HSL& lower_bound,
                      const HSL& upper_bound) {
  DCHECK(hsl.h >= 0 && hsl.h <= 1) << hsl.h;
  DCHECK(hsl.s >= 0 && hsl.s <= 1) << hsl.s;
  DCHECK(hsl.l >= 0 && hsl.l <= 1) << hsl.l;
  DCHECK(lower_bound.h < 0 || upper_bound.h < 0 ||
         (lower_bound.h <= 1 && upper_bound.h <= lower_bound.h + 1))
      << "lower_bound.h: " << lower_bound.h
      << ", upper_bound.h: " << upper_bound.h;
  DCHECK(lower_bound.s < 0 || upper_bound.s < 0 ||
         (lower_bound.s <= upper_bound.s && upper_bound.s <= 1))
      << "lower_bound.s: " << lower_bound.s
      << ", upper_bound.s: " << upper_bound.s;
  DCHECK(lower_bound.l < 0 || upper_bound.l < 0 ||
         (lower_bound.l <= upper_bound.l && upper_bound.l <= 1))
      << "lower_bound.l: " << lower_bound.l
      << ", upper_bound.l: " << upper_bound.l;

  // If the upper hue is >1, the given hue bounds wrap around at 1.
  bool matches_hue = upper_bound.h > 1
                         ? hsl.h >= lower_bound.h || hsl.h <= upper_bound.h - 1
                         : hsl.h >= lower_bound.h && hsl.h <= upper_bound.h;
  return (upper_bound.h < 0 || lower_bound.h < 0 || matches_hue) &&
         (upper_bound.s < 0 || lower_bound.s < 0 ||
          (hsl.s >= lower_bound.s && hsl.s <= upper_bound.s)) &&
         (upper_bound.l < 0 || lower_bound.l < 0 ||
          (hsl.l >= lower_bound.l && hsl.l <= upper_bound.l));
}

void MakeHSLShiftValid(HSL* hsl) {
  if (hsl->h < 0 || hsl->h > 1)
    hsl->h = -1;
  if (hsl->s < 0 || hsl->s > 1)
    hsl->s = -1;
  if (hsl->l < 0 || hsl->l > 1)
    hsl->l = -1;
}

bool IsHSLShiftMeaningful(const HSL& hsl) {
  // -1 in any channel has no effect, and 0.5 has no effect for S/L.  A shift
  // with an effective value in ANY channel is meaningful.
  return hsl.h != -1 || (hsl.s != -1 && hsl.s != 0.5) ||
         (hsl.l != -1 && hsl.l != 0.5);
}

SkColor HSLShift(SkColor color, const HSL& shift) {
  SkAlpha alpha = SkColorGetA(color);

  if (shift.h >= 0 || shift.s >= 0) {
    HSL hsl;
    SkColorToHSL(color, &hsl);

    // Replace the hue with the tint's hue.
    if (shift.h >= 0)
      hsl.h = shift.h;

    // Change the saturation.
    if (shift.s >= 0) {
      if (shift.s <= 0.5f)
        hsl.s *= shift.s * 2.0f;
      else
        hsl.s += (1.0f - hsl.s) * ((shift.s - 0.5f) * 2.0f);
    }

    color = HSLToSkColor(hsl, alpha);
  }

  if (shift.l < 0)
    return color;

  // Lightness shifts in the style of popular image editors aren't actually
  // represented in HSL - the L value does have some effect on saturation.
  float r = static_cast<float>(SkColorGetR(color));
  float g = static_cast<float>(SkColorGetG(color));
  float b = static_cast<float>(SkColorGetB(color));
  if (shift.l <= 0.5f) {
    r *= (shift.l * 2.0f);
    g *= (shift.l * 2.0f);
    b *= (shift.l * 2.0f);
  } else {
    r += (255.0f - r) * ((shift.l - 0.5f) * 2.0f);
    g += (255.0f - g) * ((shift.l - 0.5f) * 2.0f);
    b += (255.0f - b) * ((shift.l - 0.5f) * 2.0f);
  }
  return SkColorSetARGB(alpha, base::ClampRound<U8CPU>(r),
                        base::ClampRound<U8CPU>(g), base::ClampRound<U8CPU>(b));
}

SkColor AlphaBlend(SkColor foreground, SkColor background, SkAlpha alpha) {
  return AlphaBlend(foreground, background, alpha / 255.0f);
}

SkColor AlphaBlend(SkColor foreground, SkColor background, float alpha) {
  DCHECK_GE(alpha, 0.0f);
  DCHECK_LE(alpha, 1.0f);

  if (alpha == 0.0f)
    return background;
  if (alpha == 1.0f)
    return foreground;

  int f_alpha = SkColorGetA(foreground);
  int b_alpha = SkColorGetA(background);

  float normalizer = f_alpha * alpha + b_alpha * (1.0f - alpha);
  if (normalizer == 0.0f)
    return SK_ColorTRANSPARENT;

  float f_weight = f_alpha * alpha / normalizer;
  float b_weight = b_alpha * (1.0f - alpha) / normalizer;

  float r =
      SkColorGetR(foreground) * f_weight + SkColorGetR(background) * b_weight;
  float g =
      SkColorGetG(foreground) * f_weight + SkColorGetG(background) * b_weight;
  float b =
      SkColorGetB(foreground) * f_weight + SkColorGetB(background) * b_weight;

  return SkColorSetARGB(base::ClampRound<U8CPU>(normalizer),
                        base::ClampRound<U8CPU>(r), base::ClampRound<U8CPU>(g),
                        base::ClampRound<U8CPU>(b));
}

SkColor GetResultingPaintColor(SkColor foreground, SkColor background) {
  return AlphaBlend(SkColorSetA(foreground, SK_AlphaOPAQUE), background,
                    static_cast<SkAlpha>(SkColorGetA(foreground)));
}

bool IsDark(SkColor color) {
  return GetRelativeLuminance(color) < g_luminance_midpoint;
}

SkColor GetColorWithMaxContrast(SkColor color) {
  return IsDark(color) ? SK_ColorWHITE : g_darkest_color;
}

SkColor GetEndpointColorWithMinContrast(SkColor color) {
  return IsDark(color) ? g_darkest_color : SK_ColorWHITE;
}

SkColor BlendTowardMaxContrast(SkColor color, SkAlpha alpha) {
  SkAlpha original_alpha = SkColorGetA(color);
  SkColor blended_color = AlphaBlend(GetColorWithMaxContrast(color),
                                     SkColorSetA(color, SK_AlphaOPAQUE), alpha);
  return SkColorSetA(blended_color, original_alpha);
}

SkColor PickContrastingColor(SkColor foreground1,
                             SkColor foreground2,
                             SkColor background) {
  const float background_luminance = GetRelativeLuminance(background);
  return (GetContrastRatio(GetRelativeLuminance(foreground1),
                           background_luminance) >=
          GetContrastRatio(GetRelativeLuminance(foreground2),
                           background_luminance)) ?
      foreground1 : foreground2;
}

BlendResult BlendForMinContrast(
    SkColor default_foreground,
    SkColor background,
    absl::optional<SkColor> high_contrast_foreground,
    float contrast_ratio) {
  DCHECK_EQ(SkColorGetA(background), SK_AlphaOPAQUE);
  default_foreground = GetResultingPaintColor(default_foreground, background);
  if (GetContrastRatio(default_foreground, background) >= contrast_ratio)
    return {SK_AlphaTRANSPARENT, default_foreground};
  const SkColor target_foreground = GetResultingPaintColor(
      high_contrast_foreground.value_or(GetColorWithMaxContrast(background)),
      background);

  const float background_luminance = GetRelativeLuminance(background);

  SkAlpha best_alpha = SK_AlphaOPAQUE;
  SkColor best_color = target_foreground;
  // Use int for inclusive lower bound and exclusive upper bound, reserving
  // conversion to SkAlpha for the end (reduces casts).
  for (int low = SK_AlphaTRANSPARENT, high = SK_AlphaOPAQUE + 1; low < high;) {
    const SkAlpha alpha = (low + high) / 2;
    const SkColor color =
        AlphaBlend(target_foreground, default_foreground, alpha);
    const float luminance = GetRelativeLuminance(color);
    const float contrast = GetContrastRatio(luminance, background_luminance);
    if (contrast >= contrast_ratio) {
      best_alpha = alpha;
      best_color = color;
      high = alpha;
    } else {
      low = alpha + 1;
    }
  }
  return {best_alpha, best_color};
}

SkColor InvertColor(SkColor color) {
  return SkColorSetARGB(SkColorGetA(color), 255 - SkColorGetR(color),
                        255 - SkColorGetG(color), 255 - SkColorGetB(color));
}

SkColor GetSysSkColor(int which) {
#if BUILDFLAG(IS_WIN)
  return skia::COLORREFToSkColor(GetSysColor(which));
#else
  NOTIMPLEMENTED();
  return SK_ColorLTGRAY;
#endif
}

SkColor DeriveDefaultIconColor(SkColor text_color) {
  // Lighten dark colors and brighten light colors. The alpha value here (0x4c)
  // is chosen to generate a value close to GoogleGrey700 from GoogleGrey900.
  return BlendTowardMaxContrast(text_color, 0x4c);
}

std::string SkColorToRgbaString(SkColor color) {
  // We convert the alpha using NumberToString because StringPrintf will use
  // locale specific formatters (e.g., use , instead of . in German).
  return base::StringPrintf(
      "rgba(%s,%s)", SkColorToRgbString(color).c_str(),
      base::NumberToString(SkColorGetA(color) / 255.0).c_str());
}

std::string SkColorToRgbString(SkColor color) {
  return base::StringPrintf("%d,%d,%d", SkColorGetR(color), SkColorGetG(color),
                            SkColorGetB(color));
}

SkColor SetDarkestColorForTesting(SkColor color) {
  const SkColor previous_darkest_color = g_darkest_color;
  g_darkest_color = color;

  const float dark_luminance = GetRelativeLuminance(color);
  // We want to compute |g_luminance_midpoint| such that
  // GetContrastRatio(dark_luminance, g_luminance_midpoint) ==
  // GetContrastRatio(kWhiteLuminance, g_luminance_midpoint).  The formula below
  // can be verified by plugging it into how GetContrastRatio() operates.
  g_luminance_midpoint =
      std::sqrt((dark_luminance + 0.05f) * (kWhiteLuminance + 0.05f)) - 0.05f;

  return previous_darkest_color;
}

std::tuple<float, float, float> GetLuminancesForTesting() {
  return std::make_tuple(GetRelativeLuminance(g_darkest_color),
                         g_luminance_midpoint, kWhiteLuminance);
}

}  // namespace color_utils
