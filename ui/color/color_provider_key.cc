// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_provider_key.h"

#include <tuple>
#include <utility>

namespace ui {

ColorProviderKey::InitializerSupplier::InitializerSupplier() = default;

ColorProviderKey::InitializerSupplier::~InitializerSupplier() = default;

ColorProviderKey::ThemeInitializerSupplier::ThemeInitializerSupplier(
    ThemeType theme_type)
    : theme_type_(theme_type) {}

ColorProviderKey::ColorProviderKey() = default;

ColorProviderKey::ColorProviderKey(const ColorProviderKey&) = default;

ColorProviderKey& ColorProviderKey::operator=(const ColorProviderKey&) =
    default;

ColorProviderKey::ColorProviderKey(ColorProviderKey&&) = default;

ColorProviderKey& ColorProviderKey::operator=(ColorProviderKey&&) = default;

ColorProviderKey::~ColorProviderKey() = default;

bool ColorProviderKey::operator==(const ColorProviderKey& other) const {
  auto* lhs_app_controller = app_controller.get();
  auto* rhs_app_controller = other.app_controller.get();
#if BUILDFLAG(IS_ANDROID)
  return std::tie(color_mode, contrast_mode, forced_colors, system_theme,
                  frame_type, frame_style, user_color_source, user_color,
                  scheme_variant, custom_theme, lhs_app_controller,
                  app_background_color, system_theme_version, context_hash) ==
         std::tie(other.color_mode, other.contrast_mode, other.forced_colors,
                  other.system_theme, other.frame_type, other.frame_style,
                  other.user_color_source, other.user_color,
                  other.scheme_variant, other.custom_theme, rhs_app_controller,
                  other.app_background_color, other.system_theme_version,
                  other.context_hash);
#else
  return std::tie(color_mode, contrast_mode, forced_colors, system_theme,
                  frame_type, frame_style, user_color_source, user_color,
                  scheme_variant, custom_theme, lhs_app_controller,
                  app_background_color, system_theme_version) ==
         std::tie(other.color_mode, other.contrast_mode, other.forced_colors,
                  other.system_theme, other.frame_type, other.frame_style,
                  other.user_color_source, other.user_color,
                  other.scheme_variant, other.custom_theme, rhs_app_controller,
                  other.app_background_color, other.system_theme_version);
#endif
}

bool ColorProviderKey::operator<(const ColorProviderKey& other) const {
  auto* lhs_app_controller = app_controller.get();
  auto* rhs_app_controller = other.app_controller.get();
#if BUILDFLAG(IS_ANDROID)
  return std::tie(color_mode, contrast_mode, forced_colors, system_theme,
                  frame_type, frame_style, user_color_source, user_color,
                  scheme_variant, custom_theme, lhs_app_controller,
                  app_background_color, system_theme_version, context_hash) <
         std::tie(other.color_mode, other.contrast_mode, other.forced_colors,
                  other.system_theme, other.frame_type, other.frame_style,
                  other.user_color_source, other.user_color,
                  other.scheme_variant, other.custom_theme, rhs_app_controller,
                  other.app_background_color, other.system_theme_version,
                  other.context_hash);
#else
  return std::tie(color_mode, contrast_mode, forced_colors, system_theme,
                  frame_type, frame_style, user_color_source, user_color,
                  scheme_variant, custom_theme, lhs_app_controller,
                  app_background_color, system_theme_version) <
         std::tie(other.color_mode, other.contrast_mode, other.forced_colors,
                  other.system_theme, other.frame_type, other.frame_style,
                  other.user_color_source, other.user_color,
                  other.scheme_variant, other.custom_theme, rhs_app_controller,
                  other.app_background_color, other.system_theme_version);
#endif
}

}  // namespace ui
