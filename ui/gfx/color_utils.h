// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_UTILS_H_
#define UI_GFX_COLOR_UTILS_H_

#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"

class SkBitmap;

namespace color_utils {

// Represents an HSL color.
struct HSL {
  double h;
  double s;
  double l;
};

// The minimum contrast between text and background that is still readable.
// This value is taken from w3c accessibility guidelines.
constexpr float kMinimumReadableContrastRatio = 4.5f;

// Determines the contrast ratio of two colors or two relative luminance values
// (as computed by RelativeLuminance()), calculated according to
// http://www.w3.org/TR/WCAG20/#contrast-ratiodef .
GFX_EXPORT float GetContrastRatio(SkColor color_a, SkColor color_b);
GFX_EXPORT float GetContrastRatio(float luminance_a, float luminance_b);

// The relative luminance of |color|, that is, the weighted sum of the
// linearized RGB components, normalized to 0..1, per BT.709.  See
// http://www.w3.org/TR/WCAG20/#relativeluminancedef .
GFX_EXPORT float GetRelativeLuminance(SkColor color);

// The luma of |color|, that is, the weighted sum of the gamma-compressed R'G'B'
// components, per BT.601, a.k.a. the Y' in Y'UV.  See
// https://en.wikipedia.org/wiki/Luma_(video).
GFX_EXPORT uint8_t GetLuma(SkColor color);

// Note: these transformations assume sRGB as the source color space
GFX_EXPORT void SkColorToHSL(SkColor c, HSL* hsl);
GFX_EXPORT SkColor HSLToSkColor(const HSL& hsl, SkAlpha alpha);

// Determines whether the given |hsl| falls within the given range for each
// component. All components of |hsl| are expected to be in the range [0, 1].
//
// If a component is negative in either |lower_bound| or |upper_bound|, that
// component will be ignored.
//
// For hue, the lower bound should be in the range [0, 1] and the upper bound
// should be in the range [(lower bound), (lower bound + 1)].
// For saturation and value, bounds should be specified in the range [0, 1],
// with the lower bound less than the upper bound.
GFX_EXPORT bool IsWithinHSLRange(const HSL& hsl,
                                 const HSL& lower_bound,
                                 const HSL& upper_bound);

// Makes |hsl| valid input for HSLShift(). Sets values of hue, saturation
// and lightness which are outside of the valid range [0, 1] to -1.  -1 is a
// special value which indicates 'no change'.
GFX_EXPORT void MakeHSLShiftValid(HSL* hsl);

// Returns whether pasing |hsl| to HSLShift() would have any effect.  Assumes
// |hsl| is a valid shift (as defined by MakeHSLShiftValid()).
GFX_EXPORT bool IsHSLShiftMeaningful(const HSL& hsl);

// HSL-Shift an SkColor. The shift values are in the range of 0-1, with the
// option to specify -1 for 'no change'. The shift values are defined as:
// hsl_shift[0] (hue): The absolute hue value - 0 and 1 map
//    to 0 and 360 on the hue color wheel (red).
// hsl_shift[1] (saturation): A saturation shift, with the
//    following key values:
//    0 = remove all color.
//    0.5 = leave unchanged.
//    1 = fully saturate the image.
// hsl_shift[2] (lightness): A lightness shift, with the
//    following key values:
//    0 = remove all lightness (make all pixels black).
//    0.5 = leave unchanged.
//    1 = full lightness (make all pixels white).
GFX_EXPORT SkColor HSLShift(SkColor color, const HSL& shift);

// Builds a histogram based on the Y' of the Y'UV representation of this image.
GFX_EXPORT void BuildLumaHistogram(const SkBitmap& bitmap, int histogram[256]);

// Calculates how "boring" an image is. The boring score is the
// 0,1 ranged percentage of pixels that are the most common
// luma. Higher boring scores indicate that a higher percentage of a
// bitmap are all the same brightness.
GFX_EXPORT double CalculateBoringScore(const SkBitmap& bitmap);

// Returns a blend of the supplied colors, ranging from |background| (for
// |alpha| == 0) to |foreground| (for |alpha| == 255). The alpha channels of
// the supplied colors are also taken into account, so the returned color may
// be partially transparent.
GFX_EXPORT SkColor AlphaBlend(SkColor foreground, SkColor background,
                              SkAlpha alpha);

// Returns the color that results from painting |foreground| on top of
// |background|.
GFX_EXPORT SkColor GetResultingPaintColor(SkColor foreground,
                                          SkColor background);

// Returns true if the luma of |color| is closer to black than white.
GFX_EXPORT bool IsDark(SkColor color);

// Makes a dark color lighter or a light color darker by blending |color| with
// white or black depending on its current luma.  |alpha| controls the amount of
// white or black that will be alpha-blended into |color|.
GFX_EXPORT SkColor BlendTowardOppositeLuma(SkColor color, SkAlpha alpha);

// This is a copy of |getThemedAssetColor()| in ColorUtils.java.
GFX_EXPORT SkColor GetThemedAssetColor(SkColor theme_color);

// Given a foreground and background color, try to return a foreground color
// that is "readable" over the background color by luma-inverting the foreground
// color and then using PickContrastingColor() to pick the one with greater
// contrast.  During this process, alpha values will be ignored; the returned
// color will have the same alpha as |foreground|.
//
// NOTE: This won't do anything but waste time if the supplied foreground color
// has a luma value close to the midpoint (0.5 in the HSL representation).
GFX_EXPORT SkColor GetReadableColor(SkColor foreground, SkColor background);

// Returns whichever of |foreground1| or |foreground2| has higher contrast with
// |background|.
GFX_EXPORT SkColor PickContrastingColor(SkColor foreground1,
                                        SkColor foreground2,
                                        SkColor background);

// This function attempts to select a color based on |default_foreground| that
// will meet the minimum contrast ratio when used as a text color on top of
// |background|. If |default_foreground| already meets the minimum contrast
// ratio, this function will simply return it. Otherwise it will blend the color
// darker/lighter until either the contrast ratio is acceptable or the color
// cannot become any more extreme. Only use with opaque background.
GFX_EXPORT SkColor GetColorWithMinimumContrast(SkColor default_foreground,
                                               SkColor background);

// Attempts to select an alpha value such that blending |target| onto |source|
// with that alpha produces a color of at least |contrast_ratio| against |base|.
// If |source| already meets the minimum contrast ratio, this function will
// simply return 0. Otherwise it will blend the |target| onto |source| until
// either the contrast ratio is acceptable or the color cannot become any more
// extreme. |base| must be opaque.
GFX_EXPORT SkAlpha GetBlendValueWithMinimumContrast(SkColor source,
                                                    SkColor target,
                                                    SkColor base,
                                                    float contrast_ratio);

// Returns the minimum alpha value such that blending |target| onto |source|
// produces a color that contrasts against |base| with at least |contrast_ratio|
// unless this is impossible, in which case SK_AlphaOPAQUE is returned.
// Use only with opaque colors. |alpha_error_tolerance| should normally be 0 for
// best accuracy, but if performance is critical then it can be a positive value
// (4 is recommended) to save a few cycles and give "close enough" alpha.
GFX_EXPORT SkAlpha FindBlendValueForContrastRatio(SkColor source,
                                                  SkColor target,
                                                  SkColor base,
                                                  float contrast_ratio,
                                                  int alpha_error_tolerance);

// Invert a color.
GFX_EXPORT SkColor InvertColor(SkColor color);

// Gets a Windows system color as a SkColor
GFX_EXPORT SkColor GetSysSkColor(int which);

// Returns true only if Chrome should use an inverted color scheme - which is
// only true if the system has high-contrast mode enabled and and is using a
// light-on-dark color scheme.
GFX_EXPORT bool IsInvertedColorScheme();

// Derives a color for icons on a UI surface based on the text color on the same
// surface.
GFX_EXPORT SkColor DeriveDefaultIconColor(SkColor text_color);

// Creates an rgba string for an SkColor. For example: 'rgba(255,0,255,0.5)'.
GFX_EXPORT std::string SkColorToRgbaString(SkColor color);

// Creates an rgb string for an SkColor. For example: '255,0,255'.
GFX_EXPORT std::string SkColorToRgbString(SkColor color);

// Sets the color_utils darkest color to |color| from the SK_ColorBLACK default.
GFX_EXPORT void SetDarkestColor(SkColor color);

// Returns the current color_utils darkest color so tests can clean up.
GFX_EXPORT SkColor GetDarkestColor();

}  // namespace color_utils

#endif  // UI_GFX_COLOR_UTILS_H_
