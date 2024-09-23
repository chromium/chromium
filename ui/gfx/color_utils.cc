// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/color_utils.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <ostream>
#include <vector>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
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
float Linearize(float component) {
  // The W3C link in the header uses 0.03928 here.  See
  // https://en.wikipedia.org/wiki/SRGB#Theory_of_the_transformation for
  // discussion of why we use this value rather than that one.
  return (component <= 0.04045f) ? (component / 12.92f)
                                 : pow((component + 0.055f) / 1.055f, 2.4f);
}

constexpr size_t kNumGoogleColors = 12;
constexpr SkColor kGrey[kNumGoogleColors] = {
    SK_ColorWHITE,       gfx::kGoogleGrey050, gfx::kGoogleGrey100,
    gfx::kGoogleGrey200, gfx::kGoogleGrey300, gfx::kGoogleGrey400,
    gfx::kGoogleGrey500, gfx::kGoogleGrey600, gfx::kGoogleGrey700,
    gfx::kGoogleGrey800, gfx::kGoogleGrey900, gfx::kGoogleGrey900,
};

constexpr SkColor kRed[kNumGoogleColors] = {
    SK_ColorWHITE,      gfx::kGoogleRed050, gfx::kGoogleRed100,
    gfx::kGoogleRed200, gfx::kGoogleRed300, gfx::kGoogleRed400,
    gfx::kGoogleRed500, gfx::kGoogleRed600, gfx::kGoogleRed700,
    gfx::kGoogleRed800, gfx::kGoogleRed900, gfx::kGoogleGrey900,
};

constexpr SkColor kOrange[kNumGoogleColors] = {
    SK_ColorWHITE,         gfx::kGoogleOrange050, gfx::kGoogleOrange100,
    gfx::kGoogleOrange200, gfx::kGoogleOrange300, gfx::kGoogleOrange400,
    gfx::kGoogleOrange500, gfx::kGoogleOrange600, gfx::kGoogleOrange700,
    gfx::kGoogleOrange800, gfx::kGoogleOrange900, gfx::kGoogleGrey900,
};

constexpr SkColor kYellow[kNumGoogleColors] = {
    SK_ColorWHITE,         gfx::kGoogleYellow050, gfx::kGoogleYellow100,
    gfx::kGoogleYellow200, gfx::kGoogleYellow300, gfx::kGoogleYellow400,
    gfx::kGoogleYellow500, gfx::kGoogleYellow600, gfx::kGoogleYellow700,
    gfx::kGoogleYellow800, gfx::kGoogleYellow900, gfx::kGoogleGrey900,
};

constexpr SkColor kGreen[kNumGoogleColors] = {
    SK_ColorWHITE,        gfx::kGoogleGreen050, gfx::kGoogleGreen100,
    gfx::kGoogleGreen200, gfx::kGoogleGreen300, gfx::kGoogleGreen400,
    gfx::kGoogleGreen500, gfx::kGoogleGreen600, gfx::kGoogleGreen700,
    gfx::kGoogleGreen800, gfx::kGoogleGreen900, gfx::kGoogleGrey900,
};

constexpr SkColor kCyan[kNumGoogleColors] = {
    SK_ColorWHITE,       gfx::kGoogleCyan050, gfx::kGoogleCyan100,
    gfx::kGoogleCyan200, gfx::kGoogleCyan300, gfx::kGoogleCyan400,
    gfx::kGoogleCyan500, gfx::kGoogleCyan600, gfx::kGoogleCyan700,
    gfx::kGoogleCyan800, gfx::kGoogleCyan900, gfx::kGoogleGrey900,
};

constexpr SkColor kBlue[kNumGoogleColors] = {
    SK_ColorWHITE,       gfx::kGoogleBlue050, gfx::kGoogleBlue100,
    gfx::kGoogleBlue200, gfx::kGoogleBlue300, gfx::kGoogleBlue400,
    gfx::kGoogleBlue500, gfx::kGoogleBlue600, gfx::kGoogleBlue700,
    gfx::kGoogleBlue800, gfx::kGoogleBlue900, gfx::kGoogleGrey900,
};

constexpr SkColor kPurple[kNumGoogleColors] = {
    SK_ColorWHITE,         gfx::kGooglePurple050, gfx::kGooglePurple100,
    gfx::kGooglePurple200, gfx::kGooglePurple300, gfx::kGooglePurple400,
    gfx::kGooglePurple500, gfx::kGooglePurple600, gfx::kGooglePurple700,
    gfx::kGooglePurple800, gfx::kGooglePurple900, gfx::kGoogleGrey900,
};

constexpr SkColor kMagenta[kNumGoogleColors] = {
    SK_ColorWHITE,          gfx::kGoogleMagenta050, gfx::kGoogleMagenta100,
    gfx::kGoogleMagenta200, gfx::kGoogleMagenta300, gfx::kGoogleMagenta400,
    gfx::kGoogleMagenta500, gfx::kGoogleMagenta600, gfx::kGoogleMagenta700,
    gfx::kGoogleMagenta800, gfx::kGoogleMagenta900, gfx::kGoogleGrey900,
};

constexpr SkColor kPink[kNumGoogleColors] = {
    SK_ColorWHITE,       gfx::kGooglePink050, gfx::kGooglePink100,
    gfx::kGooglePink200, gfx::kGooglePink300, gfx::kGooglePink400,
    gfx::kGooglePink500, gfx::kGooglePink600, gfx::kGooglePink700,
    gfx::kGooglePink800, gfx::kGooglePink900, gfx::kGoogleGrey900,
};

SkColor PickGoogleColor(const SkColor (&colors)[kNumGoogleColors],
                        SkColor color,
                        SkColor background_color_a,
                        SkColor background_color_b,
                        float min_contrast,
                        float max_contrast_with_nearer) {
  // Sanity checks.
  DCHECK_GT(kNumGoogleColors, 0u);
  DCHECK_GE(min_contrast, 0.0f);
  DCHECK_LE(min_contrast, max_contrast_with_nearer);

  // First set up `lum_colors`, the corresponding relative luminances of
  // `colors`.  These could be precomputed and recorded next to `kGrey` etc. for
  // some runtime speedup at the cost of maintenance pain.
  float lum_colors[kNumGoogleColors];
  base::ranges::transform(colors, std::begin(lum_colors),
                          &GetRelativeLuminance);

  // This function returns an iterator to the least-contrasting luminance (in
  // `lum_colors`) to `lum`.
  const auto find_nearest_lum_it = [&lum_colors](float lum) {
    // Find the first luminance (since they're sorted decreasing) <= `lum`.
    const float* it =
        base::ranges::lower_bound(lum_colors, lum, base::ranges::greater());
    // If applicable, check against the next greater luminance for whichever is
    // lower-contrast.
    if (it == std::cend(lum_colors) ||
        ((it != std::cbegin(lum_colors)) &&
         (GetContrastRatio(lum, *it) > GetContrastRatio(*(it - 1), lum)))) {
      --it;
    }
    return it;
  };

  // Compute `src_it`, the element in `lum_colors` which is closest to `color`.
  const float* src_it = find_nearest_lum_it(GetRelativeLuminance(color));

  // Compute the background luminances.
  const bool one_bg = background_color_a == background_color_b;
  const float lum_a = GetRelativeLuminance(background_color_a);
  const float lum_b = one_bg ? lum_a : GetRelativeLuminance(background_color_b);

  // Compute `lum_mid`, the luminance between `lum_a` and `lum_b` that contrasts
  // equally with both.
  const float lum_mid =
      one_bg ? lum_a : (std::sqrt((lum_a + 0.05f) * (lum_b + 0.05f)) - 0.05f);

  // This function returns the luminance of whichever background contrasts less
  // with some given luminance (the "nearer background").
  const auto bg_lum_near_lum = [&](float lum) {
    return ((lum_a > lum_b) == (lum > lum_mid)) ? lum_a : lum_b;
  };

  // Compute the contrast of `src_it` against the nearer background.
  const float nearer_bg_lum = bg_lum_near_lum(*src_it);
  const float src_contrast_with_near = GetContrastRatio(*src_it, nearer_bg_lum);

  // This function returns the first element E, moving from `begin` towards
  // `end` (inclusive), which does not satisfy `comp(proj(E), threshold)`. In
  // other words, this is basically a direction-agnostic lower_bound().
  const auto first_across_threshold = [&](const float* begin, const float* end,
                                          float threshold, auto comp,
                                          auto proj) {
    if (end >= begin) {
      return base::ranges::lower_bound(begin, end, threshold, comp, proj);
    }
    const auto res_it_reversed = base::ranges::lower_bound(
        std::make_reverse_iterator(begin + 1),
        std::make_reverse_iterator(end + 1), threshold, comp, proj);
    return res_it_reversed.base() - 1;
  };

  // Compute `res_it`, the desired result element in `lum_colors`. Start with
  // `src_it`, then adjust depending on the contrast against the nearer
  // background.
  const float* res_it = src_it;
  if (src_contrast_with_near < min_contrast) {
    // Need to increase contrast. This will be done by iterating through
    // `lum_colors` towards a target element with sufficient contrast. The three
    // potential targets are the two endpoints and (if there are two
    // backgrounds) the element nearest `lum_mid`.
    std::vector<const float*> targets = {std::cbegin(lum_colors),
                                         std::cend(lum_colors) - 1};
    const bool src_darker_than_bg_a = *src_it < lum_a;
    if (one_bg) {
      // To avoid inverting the relationship between source and background,
      // prefer the endpoint on the "same side" of the background as the source,
      // then the other endpoint.
      if (src_darker_than_bg_a) {
        std::swap(targets[0], targets[1]);
      }
    } else if (src_darker_than_bg_a == (*src_it < lum_b)) {
      // The source is either lighter or darker than both backgrounds, so prefer
      // the endpoint on the "same side", then the midpoint, then the other
      // endpoint.
      if (src_darker_than_bg_a) {
        std::swap(targets[0], targets[1]);
      }
      targets.insert(targets.cbegin() + 1, find_nearest_lum_it(lum_mid));
    } else {
      // The source is between the two backgrounds, so prefer the midpoint, then
      // the endpoint on the "same side" of the midpoint as the source, then the
      // other endpoint.
      if (*src_it < lum_mid) {
        std::swap(targets[0], targets[1]);
      }
      targets.insert(targets.cbegin(), find_nearest_lum_it(lum_mid));
    }

    // Set `targ_it` to the first target in the priority list that has at least
    // `min_contrast` against the nearer background. If none of the targets meet
    // the contrast threshold, use the one with the best contrast.
    const float* targ_it;
    float best_contrast = 0;
    const auto proj = [&](float lum) {
      return GetContrastRatio(lum, bg_lum_near_lum(lum));
    };
    for (const float* elem : targets) {
      const float contrast = proj(*elem);
      if (contrast > best_contrast) {
        targ_it = elem;
        best_contrast = contrast;
        if (best_contrast >= min_contrast) {
          break;
        }
      }
    }

    if (best_contrast < min_contrast) {
      // Couldn't meet the threshold, so `targ_it` is the best possible result.
      res_it = targ_it;
    } else {
      // `targ_it` has sufficient contrast. Since `src_it` is already known to
      // have insufficient contrast, move it one step towards `targ_it`.
      src_it = (targ_it < src_it) ? (src_it - 1) : (src_it + 1);

      // Now keep moving towards `targ_it` until contrast is sufficient.
      res_it = first_across_threshold(src_it, targ_it, min_contrast,
                                      base::ranges::less(), proj);
    }
  } else if (src_contrast_with_near > max_contrast_with_nearer) {
    // Need to reduce contrast if possible by moving toward the nearer
    // background. Compute `targ_it`, the element in `lum_colors` whose
    // luminance is closest to the nearer background while staying on the "same
    // side" as `src_it`. (This intentionally allows `targ_it` to match the
    // nearer background's luminance exactly, in case `min_contrast == 0`.)
    const auto* targ_it =
        (*src_it > nearer_bg_lum)
            ? (std::upper_bound(src_it, std::cend(lum_colors), nearer_bg_lum,
                                std::greater<>()) -
               1)
            : std::lower_bound(std::cbegin(lum_colors), src_it, nearer_bg_lum,
                               std::greater<>());

    // Ensure `targ_it` reaches `min_contrast` against the nearer background by
    // moving toward `src_it`.
    const auto proj = [&](float lum) {
      return GetContrastRatio(lum, nearer_bg_lum);
    };
    targ_it = first_across_threshold(targ_it, src_it, min_contrast,
                                     base::ranges::less(), proj);

    // Now move `res_it` towards `targ_it` until contrast is sufficiently low.
    res_it = first_across_threshold(src_it, targ_it, max_contrast_with_nearer,
                                    base::ranges::greater(), proj);
  }

  // Convert `res_it` back to a color.
  return colors[res_it - std::begin(lum_colors)];
}

template <typename T>
SkColor PickGoogleColorImpl(SkColor color, T pick_color) {
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

}  // namespace

SkColor PickGoogleColor(SkColor color,
                        SkColor background_color,
                        float min_contrast,
                        float max_contrast) {
  const auto pick_color = [&](const SkColor(&colors)[kNumGoogleColors]) {
    return PickGoogleColor(colors, color, background_color, background_color,
                           min_contrast, max_contrast);
  };
  return PickGoogleColorImpl(color, pick_color);
}

SkColor PickGoogleColorTwoBackgrounds(SkColor color,
                                      SkColor background_color_a,
                                      SkColor background_color_b,
                                      float min_contrast,
                                      float max_contrast_with_nearer) {
  const auto pick_color = [&](const SkColor(&colors)[kNumGoogleColors]) {
    return PickGoogleColor(colors, color, background_color_a,
                           background_color_b, min_contrast,
                           max_contrast_with_nearer);
  };
  return PickGoogleColorImpl(color, pick_color);
}

float GetContrastRatio(SkColor color_a, SkColor color_b) {
  return GetContrastRatio(GetRelativeLuminance(color_a),
                          GetRelativeLuminance(color_b));
}

float GetContrastRatio(SkColor4f color_a, SkColor4f color_b) {
  return GetContrastRatio(GetRelativeLuminance4f(color_a),
                          GetRelativeLuminance4f(color_b));
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
  return GetRelativeLuminance4f(SkColor4f::FromColor(color));
}

float GetRelativeLuminance4f(SkColor4f color) {
  return (0.2126f * Linearize(color.fR)) + (0.7152f * Linearize(color.fG)) +
         (0.0722f * Linearize(color.fB));
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
  auto [vmin, vmax] = std::minmax({r, g, b});
  float delta = vmax - vmin;
  hsl->l = std::midpoint(vmin, vmax);
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

BlendResult BlendForMinContrast(SkColor default_foreground,
                                SkColor background,
                                std::optional<SkColor> high_contrast_foreground,
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
    const SkAlpha alpha = std::midpoint(low, high);
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

std::string SkColor4fToRgbaString(SkColor4f color) {
  return base::StringPrintf("rgba(%f, %f, %f, %f", color.fR, color.fG, color.fB,
                            color.fA);
}

std::string SkColorToRgbString(SkColor color) {
  return base::StringPrintf("%d,%d,%d", SkColorGetR(color), SkColorGetG(color),
                            SkColorGetB(color));
}

std::string SkColor4fToRgbString(SkColor4f color) {
  return base::StringPrintf("rgba(%f, %f, %f", color.fR, color.fG, color.fB);
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
