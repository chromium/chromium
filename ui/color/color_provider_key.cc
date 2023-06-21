// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_key.h"

#include <utility>

namespace ui {

ColorProviderKey::InitializerSupplier::InitializerSupplier() = default;

ColorProviderKey::InitializerSupplier::~InitializerSupplier() = default;

ColorProviderKey::ThemeInitializerSupplier::ThemeInitializerSupplier(
    ThemeType theme_type)
    : theme_type_(theme_type) {}

ColorProviderKey::ColorProviderKey()
    : ColorProviderKey(ColorMode::kLight,
                       ContrastMode::kNormal,
                       SystemTheme::kDefault,
                       FrameType::kChromium,
                       absl::nullopt,
                       absl::nullopt,
                       false,
                       nullptr) {}

ColorProviderKey::ColorProviderKey(
    ColorMode color_mode,
    ContrastMode contrast_mode,
    SystemTheme system_theme,
    FrameType frame_type,
    absl::optional<SkColor> user_color,
    absl::optional<SchemeVariant> scheme_variant,
    bool is_grayscale,
    scoped_refptr<ThemeInitializerSupplier> custom_theme)
    : color_mode(color_mode),
      contrast_mode(contrast_mode),
      elevation_mode(ElevationMode::kLow),
      system_theme(system_theme),
      frame_type(frame_type),
      user_color(user_color),
      scheme_variant(scheme_variant),
      is_grayscale(is_grayscale),
      custom_theme(std::move(custom_theme)) {}

ColorProviderKey::ColorProviderKey(const ColorProviderKey&) = default;

ColorProviderKey& ColorProviderKey::operator=(const ColorProviderKey&) =
    default;

ColorProviderKey::~ColorProviderKey() = default;

}  // namespace ui
