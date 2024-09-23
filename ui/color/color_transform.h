// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_TRANSFORM_H_
#define UI_COLOR_COLOR_TRANSFORM_H_

#include <optional>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_utils.h"

namespace ui {

class ColorMixer;

// Callback is a function which transforms an |input| color, optionally using a
// |mixer| (to obtain other colors). Do not depend on the callback running
// except if it's necessary for the final color.
using Callback =
    base::RepeatingCallback<SkColor(SkColor input, const ColorMixer& mixer)>;

// ColorTransform wraps Callback and can be chained together in ColorRecipes,
// where each will be applied to the preceding transform's output.
class COMPONENT_EXPORT(COLOR) ColorTransform {
 public:
  // Allows simple conversion from a Callback to a ColorTransform.
  ColorTransform(Callback callback);  // NOLINT
  // Creates a transform that returns the supplied |color|.
  ColorTransform(SkColor color);  // NOLINT
  // Creates a transform that returns the result color for the supplied |id|.
  ColorTransform(ColorId id);  // NOLINT
  ColorTransform(const ColorTransform&);
  ColorTransform& operator=(const ColorTransform&);
  ~ColorTransform();

  // Returns true if the result of this transform will return the same result
  // regardless of other transforms within the same ColorRecipe.
  bool invariant() const { return invariant_; }

  SkColor Run(SkColor input_color, const ColorMixer& mixer) const;

 private:
  Callback callback_;
  bool invariant_ = false;
};

// Functions to create common transforms:

// A transform which blends the result of |foreground_transform| atop the result
// of |background_transform| with alpha |alpha|.
COMPONENT_EXPORT(COLOR)
ColorTransform AlphaBlend(ColorTransform foreground_transform,
                          ColorTransform background_transform,
                          SkAlpha alpha);

// A transform which modifies the result of |foreground_transform| to contrast
// with the result of |background_transform| by at least |contrast_ratio|, if
// possible.  If |high_contrast_foreground_transform| is non-null, its result is
// used as the blend target.
COMPONENT_EXPORT(COLOR)
ColorTransform BlendForMinContrast(
    ColorTransform foreground_transform,
    ColorTransform background_transform,
    std::optional<ColorTransform> high_contrast_foreground_transform =
        std::nullopt,
    float contrast_ratio = color_utils::kMinimumReadableContrastRatio);

// A transform which blends the result of |transform| toward the color with max
// contrast until it has contrast of at least |contrast_ratio| with its original
// value.
COMPONENT_EXPORT(COLOR)
ColorTransform BlendForMinContrastWithSelf(ColorTransform transform,
                                           float contrast_ratio);

// A transform which blends the result of |transform| toward the color with max
// contrast by |alpha|.
COMPONENT_EXPORT(COLOR)
ColorTransform BlendTowardMaxContrast(ColorTransform transform, SkAlpha alpha);

// A transform which computes the contrast of the result of |transform| against
// the color with max contrast; then returns a color with that same contrast,
// but against the opposite endpoint.
COMPONENT_EXPORT(COLOR) ColorTransform ContrastInvert(ColorTransform transform);

// A transform which returns the default icon color for the result of
// |transform|.
COMPONENT_EXPORT(COLOR)
ColorTransform DeriveDefaultIconColor(ColorTransform transform);

// A transform which returns the transform's input color (i.e. does nothing).
// This is useful to supply as an argument to other transforms to control how
// the input color is routed.
COMPONENT_EXPORT(COLOR) ColorTransform FromTransformInput();

// A transform which returns the color with max contrast against the result of
// |transform|.
COMPONENT_EXPORT(COLOR)
ColorTransform GetColorWithMaxContrast(ColorTransform transform);

// A transform which returns the end point color with min contrast against the
// result of |transform|.
COMPONENT_EXPORT(COLOR)
ColorTransform GetEndpointColorWithMinContrast(ColorTransform transform);

// A transform which returns the resulting paint color of the result of
// |foreground_transform| over the result of |background_transform|.
COMPONENT_EXPORT(COLOR)
ColorTransform GetResultingPaintColor(ColorTransform foreground_transform,
                                      ColorTransform background_transform);

// A transform which runs one of two output transforms based on whether the
// result of |input_transform| is dark.
COMPONENT_EXPORT(COLOR)
ColorTransform SelectBasedOnDarkInput(
    ColorTransform input_transform,
    ColorTransform output_transform_for_dark_input,
    ColorTransform output_transform_for_light_input);

// A transform which sets the result of |transform| to have alpha |alpha|.
COMPONENT_EXPORT(COLOR)
ColorTransform SetAlpha(ColorTransform transform, SkAlpha alpha);

// A transform that gets a Google color with a similar hue to the result of
// `foreground_transform` and a similar contrast against the result of
// `background_transform`, subject to being at least `min_contrast` and at most
// `max_contrast`. If the result of `foreground_transform` isn't very saturated,
// grey will be used instead.
//
// Each of the following constraints takes precedence over the ones below it.
//   1. Ensure `min_contrast`, if possible, lest the UI become unreadable. If
//      there are no sufficiently-contrasting colors of the desired hue, falls
//      back to white/grey 900.
//   2. Avoid returning a lighter color than the background if the input was
//      darker, and vice versa. Inverting the relationship between foreground
//      and background could look odd.
//   3. Ensure `max_contrast`, if possible, lest some UI elements stick out too
//      much.
//   4. Adjust the relative luminance of the returned color as little as
//      possible, to minimize distortion of the intended color.
// Other than prioritizing (1), this order is subjective.
COMPONENT_EXPORT(COLOR)
ColorTransform PickGoogleColor(
    ColorTransform foreground_transform,
    ColorTransform background_transform = FromTransformInput(),
    float min_contrast = 0.0f,
    float max_contrast = color_utils::kMaximumPossibleContrast);

// Like the version above, but the constraints are modified:
//   1. Ensure `min_contrast`, if possible, with both backgrounds
//      simultaneously.
//   2. If the foreground is lighter than both backgrounds, make it lighter; if
//      it's darker than both, make it darker; if it's between the two, keep it
//      between.
//   3. Ensure `max_contrast_with_nearer` against the lower-contrast ("nearer")
//      background.
//   4. Unchanged.
COMPONENT_EXPORT(COLOR)
ColorTransform PickGoogleColorTwoBackgrounds(
    ColorTransform foreground_transform,
    ColorTransform background_a_transform,
    ColorTransform background_b_transform,
    float min_contrast,
    float max_contrast_against_nearer = color_utils::kMaximumPossibleContrast);

// A transform that returns the HSL shifted color given the input color.
COMPONENT_EXPORT(COLOR)
ColorTransform HSLShift(ColorTransform color, color_utils::HSL hsl);

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(COLOR)
ColorTransform ApplySystemControlTintIfNeeded();
#endif

}  // namespace ui

#endif  // UI_COLOR_COLOR_TRANSFORM_H_
