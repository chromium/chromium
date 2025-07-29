// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_DYNAMIC_COLOR_PALETTE_FACTORY_H_
#define UI_COLOR_DYNAMIC_COLOR_PALETTE_FACTORY_H_

#include <memory>
#include <optional>

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/dynamic_color/palette.h"

namespace ui {

// For the desired `seed_color` and `variant`, generates the correct type of
// `Palette`.
COMPONENT_EXPORT(DYNAMIC_COLOR)
std::unique_ptr<Palette> GeneratePalette(
    SkColor seed_color,
    ColorProviderKey::SchemeVariant variant);

// Represents the number of shades needed to be generated for each hue specified
// in the `tab_group_color_palette` key of `theme` key in manifest.json.
inline constexpr size_t kGeneratedShadesCount = 11;

// This function attempts to dynamically generate `kGeneratedShadesCount`
// standard shades(50, 100, 200, ..., 1000) from a seed |hue| which are similar
// to the shades hard-coded in the file "ui/gfx/color_palette.h". To achieve
// this we use pre-defined Chroma and Tone values which are derived from the
// same hard-coded shades in the mentioned file.
//
// It takes an optional |hue| in the range [0,360]. If |hue| is std::nullopt,
// grey shades will be generated instead.
COMPONENT_EXPORT(DYNAMIC_COLOR)
void GenerateStandardShadesFromHue(
    std::optional<int> hue,
    std::array<SkColor, kGeneratedShadesCount>& shades);

}  // namespace ui

#endif  // UI_COLOR_DYNAMIC_COLOR_PALETTE_FACTORY_H_
