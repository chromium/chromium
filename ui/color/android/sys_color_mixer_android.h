// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_ANDROID_SYS_COLOR_MIXER_ANDROID_H_
#define UI_COLOR_ANDROID_SYS_COLOR_MIXER_ANDROID_H_

#include <array>
#include <utility>

#include "base/component_export.h"
#include "ui/color/android/android_color_roles.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_key.h"

namespace ui {

class ColorProvider;

namespace internal {

// Maps Android theme color roles to the ColorIds they populate. Exposed in the
// header only for tests; use AddSysColorMixerAndroid() instead.
// TODO(crbug.com/537488418, crbug.com/537488598): Add missing color mappings
// and move mapping under ui/color.
inline constexpr auto kAndroidColorRoleToColorId =
    std::to_array<std::pair<AndroidColorRole, ColorId>>({
        {AndroidColorRole::kPrimary, kColorSysPrimary},
        {AndroidColorRole::kOnPrimary, kColorSysOnPrimary},
        {AndroidColorRole::kPrimaryContainer, kColorSysPrimaryContainer},
        {AndroidColorRole::kOnPrimaryContainer, kColorSysOnPrimaryContainer},
        {AndroidColorRole::kSecondary, kColorSysSecondary},
        {AndroidColorRole::kOnSecondary, kColorSysOnSecondary},
        {AndroidColorRole::kSecondaryContainer, kColorSysSecondaryContainer},
        {AndroidColorRole::kOnSecondaryContainer,
         kColorSysOnSecondaryContainer},
        {AndroidColorRole::kTertiary, kColorSysTertiary},
        {AndroidColorRole::kOnTertiary, kColorSysOnTertiary},
        {AndroidColorRole::kTertiaryContainer, kColorSysTertiaryContainer},
        {AndroidColorRole::kOnTertiaryContainer, kColorSysOnTertiaryContainer},
        {AndroidColorRole::kBackground, kColorSysBase},
        {AndroidColorRole::kSurface, kColorSysSurface},
        {AndroidColorRole::kOnSurface, kColorSysOnSurface},
        {AndroidColorRole::kSurfaceVariant, kColorSysSurfaceVariant},
        {AndroidColorRole::kOnSurfaceVariant, kColorSysOnSurfaceVariant},
        {AndroidColorRole::kOutline, kColorSysOutline},
        {AndroidColorRole::kError, kColorSysError},
        {AndroidColorRole::kOnError, kColorSysOnError},
        {AndroidColorRole::kErrorContainer, kColorSysErrorContainer},
        {AndroidColorRole::kOnErrorContainer, kColorSysOnErrorContainer},
        {AndroidColorRole::kInverseSurface, kColorSysInverseSurface},
        {AndroidColorRole::kInverseOnSurface, kColorSysInverseOnSurface},
        {AndroidColorRole::kInversePrimary, kColorSysInversePrimary},
        {AndroidColorRole::kSurfaceContainerLowest,
         kColorSysSurfaceContainerLowest},
        {AndroidColorRole::kSurfaceContainerLow, kColorSysSurfaceContainerLow},
        {AndroidColorRole::kSurfaceContainer, kColorSysSurfaceContainer},
        {AndroidColorRole::kSurfaceContainerHigh,
         kColorSysSurfaceContainerHigh},
        {AndroidColorRole::kSurfaceContainerHighest,
         kColorSysSurfaceContainerHighest},
    });

}  // namespace internal

COMPONENT_EXPORT(COLOR)
void AddSysColorMixerAndroid(ColorProvider* provider,
                             const ColorProviderKey& key);

}  // namespace ui

#endif  // UI_COLOR_ANDROID_SYS_COLOR_MIXER_ANDROID_H_
