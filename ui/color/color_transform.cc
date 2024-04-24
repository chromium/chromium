// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_transform.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_utils.h"

namespace ui {

ColorTransform::ColorTransform(Callback callback)
    : callback_(std::move(callback)) {}

ColorTransform::ColorTransform(SkColor color) : invariant_(true) {
  const auto generator = [](SkColor color, SkColor input_color,
                            const ColorMixer& mixer) {
    DVLOG(2) << "ColorTransform From Color:"
             << " Input Color:" << SkColorName(input_color)
             << " Color: " << SkColorName(color)
             << " Result Color: " << SkColorName(color);
    return color;
  };
  callback_ = base::BindRepeating(generator, color);
}

ColorTransform::ColorTransform(ColorId id) : invariant_(true) {
  const auto generator = [](ColorId id, SkColor input_color,
                            const ColorMixer& mixer) {
    SkColor result_color = mixer.GetResultColor(id);
    DVLOG(2) << "ColorTransform FromMixer:"
             << " Input Color:" << SkColorName(input_color)
             << " Color Id: " << ColorIdName(id)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  callback_ = base::BindRepeating(generator, id);
}

ColorTransform::ColorTransform(const ColorTransform&) = default;

ColorTransform& ColorTransform::operator=(const ColorTransform&) = default;

ColorTransform::~ColorTransform() = default;

SkColor ColorTransform::Run(SkColor input_color,
                            const ColorMixer& mixer) const {
  return callback_.Run(input_color, mixer);
}

ColorTransform AlphaBlend(ColorTransform foreground_transform,
                          ColorTransform background_transform,
                          SkAlpha alpha) {
  const auto generator = [](ColorTransform foreground_transform,
                            ColorTransform background_transform, SkAlpha alpha,
                            SkColor input_color, const ColorMixer& mixer) {
    const SkColor foreground_color =
        foreground_transform.Run(input_color, mixer);
    const SkColor background_color =
        background_transform.Run(input_color, mixer);
    const SkColor result_color =
        color_utils::AlphaBlend(foreground_color, background_color, alpha);
    DVLOG(2) << "ColorTransform AlphaBlend:"
             << " Input Color: " << SkColorName(input_color)
             << " FG Transform: " << SkColorName(foreground_color)
             << " BG Transform: " << SkColorName(background_color)
             << " Alpha: " << base::NumberToString(alpha)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(foreground_transform),
                             std::move(background_transform), alpha);
}

ColorTransform BlendForMinContrast(
    ColorTransform foreground_transform,
    ColorTransform background_transform,
    std::optional<ColorTransform> high_contrast_foreground_transform,
    float contrast_ratio) {
  const auto generator =
      [](ColorTransform foreground_transform,
         ColorTransform background_transform,
         std::optional<ColorTransform> high_contrast_foreground_transform,
         float contrast_ratio, SkColor input_color, const ColorMixer& mixer) {
        const SkColor foreground_color =
            foreground_transform.Run(input_color, mixer);
        const SkColor background_color =
            background_transform.Run(input_color, mixer);
        const std::optional<SkColor> high_contrast_foreground =
            high_contrast_foreground_transform.has_value()
                ? std::make_optional(
                      high_contrast_foreground_transform.value().Run(
                          input_color, mixer))
                : std::nullopt;
        const SkColor result_color =
            color_utils::BlendForMinContrast(foreground_color, background_color,
                                             high_contrast_foreground,
                                             contrast_ratio)
                .color;
        DVLOG(2) << "ColorTransform BlendForMinContrast:"
                 << " FG Transform Color: " << SkColorName(foreground_color)
                 << " BG Transform Color: " << SkColorName(background_color)
                 << " High Contrast Foreground: "
                 << (high_contrast_foreground.has_value()
                         ? SkColorName(high_contrast_foreground.value())
                         : "<none>")
                 << " Contrast Ratio: " << base::NumberToString(contrast_ratio)
                 << " Result Color: " << SkColorName(result_color);
        return result_color;
      };
  return base::BindRepeating(generator, std::move(foreground_transform),
                             std::move(background_transform),
                             std::move(high_contrast_foreground_transform),
                             contrast_ratio);
}

ColorTransform BlendForMinContrastWithSelf(ColorTransform transform,
                                           float contrast_ratio) {
  return BlendForMinContrast(transform, transform, std::nullopt,
                             contrast_ratio);
}

ColorTransform BlendTowardMaxContrast(ColorTransform transform, SkAlpha alpha) {
  const auto generator = [](ColorTransform transform, SkAlpha alpha,
                            SkColor input_color, const ColorMixer& mixer) {
    const SkColor transform_color = transform.Run(input_color, mixer);
    const SkColor result_color =
        color_utils::BlendTowardMaxContrast(transform_color, alpha);
    DVLOG(2) << "ColorTransform BlendTowardMaxContrast:"
             << " Input Color:" << SkColorName(input_color)
             << " Transform Color: " << SkColorName(transform_color)
             << " Alpha: " << base::NumberToString(alpha)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(transform), alpha);
}

ColorTransform ContrastInvert(ColorTransform transform) {
  const auto generator = [](ColorTransform transform, SkColor input_color,
                            const ColorMixer& mixer) {
    const SkColor foreground = transform.Run(input_color, mixer);
    const SkColor far_endpoint =
        color_utils::GetColorWithMaxContrast(foreground);
    const SkColor near_endpoint =
        color_utils::GetEndpointColorWithMinContrast(foreground);
    const float contrast_ratio =
        color_utils::GetContrastRatio(foreground, far_endpoint);
    const SkColor result_color =
        color_utils::BlendForMinContrast(foreground, near_endpoint,
                                         std::nullopt, contrast_ratio)
            .color;
    DVLOG(2) << "ColorTransform ContrastInvert:"
             << " Input Color: " << SkColorName(input_color)
             << " Foreground: " << SkColorName(foreground)
             << " Far End: " << SkColorName(far_endpoint)
             << " Near End: " << SkColorName(near_endpoint)
             << " Contrast Ratio: " << base::NumberToString(contrast_ratio)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(transform));
}

ColorTransform DeriveDefaultIconColor(ColorTransform transform) {
  const auto generator = [](ColorTransform transform, SkColor input_color,
                            const ColorMixer& mixer) {
    const SkColor transform_color = transform.Run(input_color, mixer);
    const SkColor result_color =
        color_utils::DeriveDefaultIconColor(transform_color);
    DVLOG(2) << "ColorTransform DeriveDefaultIconColor:"
             << " Input Color: " << SkColorName(input_color)
             << " Transform Color: " << SkColorName(transform_color)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(transform));
}

ColorTransform FromTransformInput() {
  const auto generator = [](SkColor input_color, const ColorMixer& mixer) {
    DVLOG(2) << "ColorTransform FromTransformInput: "
             << " Input/Result Color: " << SkColorName(input_color);
    return input_color;
  };
  return base::BindRepeating(generator);
}

ColorTransform GetColorWithMaxContrast(ColorTransform transform) {
  const auto generator = [](ColorTransform transform, SkColor input_color,
                            const ColorMixer& mixer) {
    const SkColor transform_color = transform.Run(input_color, mixer);
    const SkColor result_color =
        color_utils::GetColorWithMaxContrast(transform_color);
    DVLOG(2) << "ColorTransform GetColorWithMaxContrast:"
             << " Input Color: " << SkColorName(input_color)
             << " Transform Color: " << SkColorName(transform_color)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(transform));
}

ColorTransform GetEndpointColorWithMinContrast(ColorTransform transform) {
  const auto generator = [](ColorTransform transform, SkColor input_color,
                            const ColorMixer& mixer) {
    const SkColor transform_color = transform.Run(input_color, mixer);
    const SkColor result_color =
        color_utils::GetEndpointColorWithMinContrast(transform_color);
    DVLOG(2) << "ColorTransform GetEndPointColorWithMinContrast:"
             << " Input Color: " << SkColorName(input_color)
             << " Transform Color: " << SkColorName(transform_color)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(transform));
}

ColorTransform GetResultingPaintColor(ColorTransform foreground_transform,
                                      ColorTransform background_transform) {
  const auto generator = [](ColorTransform foreground_transform,
                            ColorTransform background_transform,
                            SkColor input_color, const ColorMixer& mixer) {
    const SkColor foreground_color =
        foreground_transform.Run(input_color, mixer);
    const SkColor background_color =
        background_transform.Run(input_color, mixer);
    const SkColor result_color =
        color_utils::GetResultingPaintColor(foreground_color, background_color);
    DVLOG(2) << "ColorTransform GetResultingPaintColor:"
             << " Input Color: " << SkColorName(input_color)
             << " FG Transform Color: " << SkColorName(foreground_color)
             << " BG Transform Color: " << SkColorName(background_color)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(foreground_transform),
                             std::move(background_transform));
}

ColorTransform SelectBasedOnDarkInput(
    ColorTransform input_transform,
    ColorTransform output_transform_for_dark_input,
    ColorTransform output_transform_for_light_input) {
  const auto generator = [](ColorTransform input_transform,
                            ColorTransform output_transform_for_dark_input,
                            ColorTransform output_transform_for_light_input,
                            SkColor input_color, const ColorMixer& mixer) {
    const SkColor color = input_transform.Run(input_color, mixer);
    const auto& output_transform = color_utils::IsDark(color)
                                       ? output_transform_for_dark_input
                                       : output_transform_for_light_input;
    const SkColor result_color = output_transform.Run(input_color, mixer);
    DVLOG(2) << "ColorTransform SelectBasedOnDarkInput:"
             << " Input Color: " << SkColorName(input_color)
             << " Input Transform: " << SkColorName(color)
             << " IsDark: " << (color_utils::IsDark(color) ? "true" : "false")
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(input_transform),
                             std::move(output_transform_for_dark_input),
                             std::move(output_transform_for_light_input));
}

ColorTransform SetAlpha(ColorTransform transform, SkAlpha alpha) {
  const auto generator = [](ColorTransform transform, SkAlpha alpha,
                            SkColor input_color, const ColorMixer& mixer) {
    const SkColor transform_color = transform.Run(input_color, mixer);
    const SkColor result_color = SkColorSetA(transform_color, alpha);
    DVLOG(2) << "ColorTransform SetAlpha:"
             << " Input Color: " << SkColorName(input_color)
             << " Transform Color: " << SkColorName(transform_color)
             << " Alpha: " << base::NumberToString(alpha)
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(transform), alpha);
}

ColorTransform PickGoogleColor(ColorTransform foreground_transform,
                               ColorTransform background_transform,
                               float min_contrast,
                               float max_contrast) {
  const auto generator =
      [](ColorTransform foreground_transform,
         ColorTransform background_transform, float min_contrast,
         float max_contrast, SkColor input_color, const ColorMixer& mixer) {
        const SkColor foreground_color =
            foreground_transform.Run(input_color, mixer);
        const SkColor background_color =
            background_transform.Run(input_color, mixer);
        const SkColor result_color = color_utils::PickGoogleColor(
            foreground_color, background_color, min_contrast, max_contrast);
        DVLOG(2) << "ColorTransform PickGoogleColor:"
                 << " Input Color: " << SkColorName(input_color)
                 << " Foreground Color: " << SkColorName(foreground_color)
                 << " Background Color: " << SkColorName(background_color)
                 << " Min Contrast: " << base::NumberToString(min_contrast)
                 << " Max Contrast: " << base::NumberToString(max_contrast)
                 << " Result Color: " << SkColorName(result_color);
        return result_color;
      };
  return base::BindRepeating(generator, std::move(foreground_transform),
                             std::move(background_transform), min_contrast,
                             max_contrast);
}

ColorTransform PickGoogleColorTwoBackgrounds(
    ColorTransform foreground_transform,
    ColorTransform background_a_transform,
    ColorTransform background_b_transform,
    float min_contrast,
    float max_contrast_against_nearer) {
  const auto generator = [](ui::ColorTransform foreground_transform,
                            ui::ColorTransform background_a_transform,
                            ui::ColorTransform background_b_transform,
                            float min_contrast,
                            float max_contrast_against_nearer,
                            SkColor input_color, const ui::ColorMixer& mixer) {
    const SkColor foreground_color =
        foreground_transform.Run(input_color, mixer);
    const SkColor background_a_color =
        background_a_transform.Run(input_color, mixer);
    const SkColor background_b_color =
        background_b_transform.Run(input_color, mixer);
    const SkColor result_color = color_utils::PickGoogleColorTwoBackgrounds(
        foreground_color, background_a_color, background_b_color, min_contrast,
        max_contrast_against_nearer);
    DVLOG(2) << "ColorTransform PickGoogleColor:"
             << " Foreground Color: " << ui::SkColorName(foreground_color)
             << " Background Color A: " << ui::SkColorName(background_a_color)
             << " Background Color B: " << ui::SkColorName(background_b_color)
             << " Min Contrast: " << base::NumberToString(min_contrast)
             << " Max Contrast Against Nearer: "
             << base::NumberToString(max_contrast_against_nearer)
             << " Result Color: " << ui::SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(foreground_transform),
                             std::move(background_a_transform),
                             std::move(background_b_transform), min_contrast,
                             max_contrast_against_nearer);
}

ColorTransform HSLShift(ColorTransform color, color_utils::HSL hsl) {
  const auto generator = [](ColorTransform transform, color_utils::HSL hsl,
                            SkColor input_color, const ColorMixer& mixer) {
    const SkColor transform_color = transform.Run(input_color, mixer);
    const SkColor result_color = color_utils::HSLShift(transform_color, hsl);
    DVLOG(2) << "ColorTransform HSLShift:"
             << " Input Color: " << SkColorName(input_color)
             << " Transform Color: " << SkColorName(transform_color)
             << " HSL: {" << base::NumberToString(hsl.h) << ", "
             << base::NumberToString(hsl.s) << ", "
             << base::NumberToString(hsl.l) << "}"
             << " Result Color: " << SkColorName(result_color);
    return result_color;
  };
  return base::BindRepeating(generator, std::move(color), hsl);
}

}  // namespace ui
