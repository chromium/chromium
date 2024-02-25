// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COLOR_UTILS_H_
#define UI_GFX_COLOR_UTILS_H_

#include <optional>
#include <string>
#include <tuple>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/gfx_export.h"

namespace color_utils {

// Represents an HSL color.
struct HSL {
  double h;
  double s;
  double l;
};

// The blend alpha and resulting color when blending to achieve a desired
// contrast raio.
struct BlendResult {
  SkAlpha alpha;
  SkColor color;
};

// The maximum contrast that can be achieved (i.e. white against black).
constexpr float kMaximumPossibleContrast = 21.0f;

// The minimum contrast between text and background that is still readable.
// This value is taken from w3c accessibility guidelines.
constexpr float kMinimumReadableContrastRatio = 4.5f;

// The minimum contrast between button glyphs, focus indicators, large text, or
// other "have to see it but perhaps don't have to read fine detail" cases and
// background.
constexpr float kMinimumVisibleContrastRatio = 3.0f;

// Determines the contrast ratio of two colors or two relative luminance values
// (as computed by RelativeLuminance()), calculated according to
// http://www.w3.org/TR/WCAG20/#contrast-ratiodef .
GFX_EXPORT float GetContrastRatio(SkColor4f color_a, SkColor4f color_b);
GFX_EXPORT float GetContrastRatio(SkColor color_a, SkColor color_b);
GFX_EXPORT float GetContrastRatio(float luminance_a, float luminance_b);

// The relative luminance of |color|, that is, the weighted sum of the
// linearized RGB components, normalized to 0..1, per BT.709.  See
// http://www.w3.org/TR/WCAG20/#relativeluminancedef .
GFX_EXPORT float GetRelativeLuminance4f(SkColor4f color);
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

// Returns a blend of the supplied colors, ranging from |background| (for
// |alpha| == 0) to |foreground| (for |alpha| == 255). The alpha channels of
// the supplied colors are also taken into account, so the returned color may
// be partially transparent.
GFX_EXPORT SkColor AlphaBlend(SkColor foreground,
                              SkColor background,
                              SkAlpha alpha);

// As above, but with alpha specified as 0..1.
GFX_EXPORT SkColor AlphaBlend(SkColor foreground,
                              SkColor background,
                              float alpha);

// Returns the color that results from painting |foreground| on top of
// |background|.
GFX_EXPORT SkColor GetResultingPaintColor(SkColor foreground,
                                          SkColor background);

// Returns true if |color| contrasts more with white than the darkest color.
GFX_EXPORT bool IsDark(SkColor color);

// Returns whichever of white or the darkest available color contrasts more with
// |color|.
GFX_EXPORT SkColor GetColorWithMaxContrast(SkColor color);

// Returns whichever of white or the darkest available color contrasts less with
// |color|.
GFX_EXPORT SkColor GetEndpointColorWithMinContrast(SkColor color);

// Blends towards the color with max contrast by |alpha|. The alpha of
// the original color is preserved.
GFX_EXPORT SkColor BlendTowardMaxContrast(SkColor color, SkAlpha alpha);

// Returns whichever of |foreground1| or |foreground2| has higher contrast with
// |background|.
GFX_EXPORT SkColor PickContrastingColor(SkColor foreground1,
                                        SkColor foreground2,
                                        SkColor background);

// Alpha-blends |default_foreground| toward either |high_contrast_foreground|
// (if specified) or the color with max contrast with |background| until either
// the result has a contrast ratio against |background| of at least
// |contrast_ratio| or the blend can go no further.  Returns the blended color
// and the alpha used to achieve that blend.  If |default_foreground| already
// has sufficient contrast, returns an alpha of 0 and color of
// |default_foreground|.
GFX_EXPORT BlendResult BlendForMinContrast(
    SkColor default_foreground,
    SkColor background,
    std::optional<SkColor> high_contrast_foreground = std::nullopt,
    float contrast_ratio = kMinimumReadableContrastRatio);

// Invert a color.
GFX_EXPORT SkColor InvertColor(SkColor color);

// Gets a Windows system color as a SkColor
GFX_EXPORT SkColor GetSysSkColor(int which);

// Derives a color for icons on a UI surface based on the text color on the same
// surface.
GFX_EXPORT SkColor DeriveDefaultIconColor(SkColor text_color);

// Gets a Google color with a similar hue to `color` and a similar contrast
// against `background_color`, subject to being at least `min_contrast` and at
// most `max_contrast`. If `color` isn't very saturated, grey will be used
// instead.
//
// Each of the following constraints takes precedence over the ones below it.
//   1. Ensure `min_contrast`, if possible, lest the UI become unreadable. If
//      there are no sufficiently-contrasting colors of the desired hue, falls
//      back to white/grey 900.
//   2. Avoid returning a lighter color than the background if the input was
//      darker, and vice versa. Inverting the relationship between `color` and
//      `background_color` could look odd.
//   3. Ensure `max_contrast`, if possible, lest some UI elements stick out too
//      much.
//   4. Adjust the relative luminance of the returned color as little as
//      possible, to minimize distortion of the intended color.
// Other than prioritizing (1), this order is subjective.
GFX_EXPORT SkColor
PickGoogleColor(SkColor color,
                SkColor background_color,
                float min_contrast,
                float max_contrast = kMaximumPossibleContrast);

// Like the version above, but the constraints are modified:
//   1. Ensure `min_contrast`, if possible, with both backgrounds
//      simultaneously.
//   2. If the input is lighter than both backgrounds, make it lighter; if it's
//      darker than both, make it darker; if it's between the two, keep it
//      between.
//   3. Ensure `max_contrast_with_nearer` against the lower-contrast ("nearer")
//      background.
//   4. Unchanged.
GFX_EXPORT SkColor PickGoogleColorTwoBackgrounds(
    SkColor color,
    SkColor background_color_a,
    SkColor background_color_b,
    float min_contrast,
    float max_contrast_with_nearer = kMaximumPossibleContrast);

// Creates an rgba string for an SkColor. For example: 'rgba(255,0,255,0.5)'.
GFX_EXPORT std::string SkColorToRgbaString(SkColor color);
GFX_EXPORT std::string SkColor4fToRgbaString(SkColor4f color);

// Creates an rgb string for an SkColor. For example: '255,0,255'.
GFX_EXPORT std::string SkColorToRgbString(SkColor color);
GFX_EXPORT std::string SkColor4fToRgbString(SkColor4f color);

// Sets the darkest available color to |color|.  Returns the previous darkest
// color.
GFX_EXPORT SkColor SetDarkestColorForTesting(SkColor color);

// Returns the luminance of the darkest, midpoint, and lightest colors.
GFX_EXPORT std::tuple<float, float, float> GetLuminancesForTesting();

}  // namespace color_utils

#endif  // UI_GFX_COLOR_UTILS_H_
