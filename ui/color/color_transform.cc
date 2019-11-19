// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_transform.h"

#include "base/bind.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_mixer.h"

namespace ui {

ColorTransform::ColorTransform(Callback callback)
    : callback_(std::move(callback)) {}

ColorTransform::ColorTransform(SkColor color) {
  const auto generator = [](SkColor color, SkColor input_color,
                            const ColorMixer& mixer) { return color; };
  callback_ = base::Bind(generator, color);
}

ColorTransform::ColorTransform(ColorId id) {
  DCHECK_COLOR_ID_VALID(id);
  const auto generator = [](ColorId id, SkColor input_color,
                            const ColorMixer& mixer) {
    return mixer.GetResultColor(id);
  };
  callback_ = base::Bind(generator, id);
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
    return color_utils::AlphaBlend(foreground_transform.Run(input_color, mixer),
                                   background_transform.Run(input_color, mixer),
                                   alpha);
  };
  return base::Bind(generator, std::move(foreground_transform),
                    std::move(background_transform), alpha);
}

ColorTransform BlendForMinContrast(
    ColorTransform foreground_transform,
    ColorTransform background_transform,
    base::Optional<ColorTransform> high_contrast_foreground_transform,
    float contrast_ratio) {
  const auto generator =
      [](ColorTransform foreground_transform,
         ColorTransform background_transform,
         base::Optional<ColorTransform> high_contrast_foreground_transform,
         float contrast_ratio, SkColor input_color, const ColorMixer& mixer) {
        const SkColor foreground_color =
            foreground_transform.Run(input_color, mixer);
        const SkColor background_color =
            background_transform.Run(input_color, mixer);
        const base::Optional<SkColor> high_contrast_foreground =
            high_contrast_foreground_transform.has_value()
                ? base::make_optional(
                      high_contrast_foreground_transform.value().Run(
                          input_color, mixer))
                : base::nullopt;
        return color_utils::BlendForMinContrast(
                   foreground_color, background_color, high_contrast_foreground,
                   contrast_ratio)
            .color;
      };
  return base::Bind(generator, std::move(foreground_transform),
                    std::move(background_transform),
                    std::move(high_contrast_foreground_transform),
                    contrast_ratio);
}

ColorTransform BlendForMinContrastWithSelf(ColorTransform transform,
                                           float contrast_ratio) {
  return BlendForMinContrast(transform, transform, base::nullopt,
                             contrast_ratio);
}

ColorTransform BlendTowardMaxContrast(ColorTransform transform, SkAlpha alpha) {
  const auto generator = [](ColorTransform transform, SkAlpha alpha,
                            SkColor input_color, const ColorMixer& mixer) {
    return color_utils::BlendTowardMaxContrast(
        transform.Run(input_color, mixer), alpha);
  };
  return base::Bind(generator, std::move(transform), alpha);
}

ColorTransform ContrastInvert(ColorTransform transform) {
  const auto generator = [](ColorTransform transform, SkColor input_color,
                            const ColorMixer& mixer) {
    const SkColor foreground = transform.Run(input_color, mixer);
    const SkColor far_endpoint =
        color_utils::GetColorWithMaxContrast(foreground);
    const SkColor near_endpoint =
        color_utils::GetColorWithMaxContrast(far_endpoint);
    const float contrast_ratio =
        color_utils::GetContrastRatio(foreground, far_endpoint);
    return color_utils::BlendForMinContrast(foreground, near_endpoint,
                                            base::nullopt, contrast_ratio)
        .color;
  };
  return base::Bind(generator, std::move(transform));
}

ColorTransform DeriveDefaultIconColor(ColorTransform transform) {
  const auto generator = [](ColorTransform transform, SkColor input_color,
                            const ColorMixer& mixer) {
    return color_utils::DeriveDefaultIconColor(
        transform.Run(input_color, mixer));
  };
  return base::Bind(generator, std::move(transform));
}

ColorTransform FromOriginalColorFromSet(ColorId id, ColorSetId set_id) {
  DCHECK_COLOR_ID_VALID(id);
  DCHECK_COLOR_SET_ID_VALID(set_id);
  const auto generator = [](ColorId id, ColorSetId set_id, SkColor input_color,
                            const ColorMixer& mixer) {
    return mixer.GetOriginalColorFromSet(id, set_id);
  };
  return base::Bind(generator, id, set_id);
}

ColorTransform FromTransformInput() {
  const auto generator = [](SkColor input_color, const ColorMixer& mixer) {
    return input_color;
  };
  return base::Bind(generator);
}

ColorTransform GetColorWithMaxContrast(ColorTransform transform) {
  const auto generator = [](ColorTransform transform, SkColor input_color,
                            const ColorMixer& mixer) {
    return color_utils::GetColorWithMaxContrast(
        transform.Run(input_color, mixer));
  };
  return base::Bind(generator, std::move(transform));
}

ColorTransform GetResultingPaintColor(ColorTransform foreground_transform,
                                      ColorTransform background_transform) {
  const auto generator = [](ColorTransform foreground_transform,
                            ColorTransform background_transform,
                            SkColor input_color, const ColorMixer& mixer) {
    return color_utils::GetResultingPaintColor(
        foreground_transform.Run(input_color, mixer),
        background_transform.Run(input_color, mixer));
  };
  return base::Bind(generator, std::move(foreground_transform),
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
    return output_transform.Run(input_color, mixer);
  };
  return base::Bind(generator, std::move(input_transform),
                    std::move(output_transform_for_dark_input),
                    std::move(output_transform_for_light_input));
}

ColorTransform SetAlpha(ColorTransform transform, SkAlpha alpha) {
  const auto generator = [](ColorTransform transform, SkAlpha alpha,
                            SkColor input_color, const ColorMixer& mixer) {
    return SkColorSetA(transform.Run(input_color, mixer), alpha);
  };
  return base::Bind(generator, std::move(transform), alpha);
}

}  // namespace ui
