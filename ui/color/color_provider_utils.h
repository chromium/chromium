// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_UTILS_H_
#define UI_COLOR_COLOR_PROVIDER_UTILS_H_

#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/strings/string_piece.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_id.mojom.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_manager.h"

namespace ui {

using RendererColorMap = base::flat_map<color::mojom::RendererColorId, SkColor>;

class COMPONENT_EXPORT(COLOR) ColorProviderUtilsCallbacks {
 public:
  virtual ~ColorProviderUtilsCallbacks();
  virtual bool ColorIdName(ColorId color_id, base::StringPiece* color_name) = 0;
};

// The following functions convert various values to strings intended for
// logging. Do not retain the results for longer than the scope in which these
// functions are called.

// Converts the ColorMode.
base::StringPiece COMPONENT_EXPORT(COLOR)
    ColorModeName(ColorProviderKey::ColorMode color_mode);

// Converts the ContrastMode.
base::StringPiece COMPONENT_EXPORT(COLOR)
    ContrastModeName(ColorProviderKey::ContrastMode contrast_mode);

// Converts SystemTheme.
base::StringPiece COMPONENT_EXPORT(COLOR)
    SystemThemeName(ui::SystemTheme system_theme);

// Converts ColorId.
std::string COMPONENT_EXPORT(COLOR) ColorIdName(ColorId color_id);

// Converts SkColor to string. Check if color matches a standard color palette
// value and return it as a string. Otherwise return as an rgba(xx, xxx, xxx,
// xxx) string.
std::string COMPONENT_EXPORT(COLOR) SkColorName(SkColor color);

// Converts Color Provider Color Id in string format from kColorXXX to
// "--color-X-X-X" for CSS
std::string COMPONENT_EXPORT(COLOR)
    ConvertColorProviderColorIdToCSSColorId(std::string color_id_name);

// Converts SkColor in ARGB format to CSS color in RGBA color. Returns the color
// in a Hex string representation.
std::string COMPONENT_EXPORT(COLOR) ConvertSkColorToCSSColor(SkColor color);

// Creates a map of RendererColorIds to SkColors from `color_provider`. This is
// used when sending ColorProvider colors to renderer processes. Sending a map
// keyed with RendererColorIds (as opposed to ColorIds) allows us to validate
// the ids that are sent to the renderer.
RendererColorMap COMPONENT_EXPORT(COLOR)
    CreateRendererColorMap(const ColorProvider& color_provider);

// Used in combination with CreateRendererColormap() to create the ColorProvider
// in the renderer process.
ColorProvider COMPONENT_EXPORT(COLOR) CreateColorProviderFromRendererColorMap(
    const RendererColorMap& renderer_color_map);

// Creates a color provider emulating Windows 10 default high contrast color
// themes. Currently only defines colors for scrollbar parts.
ColorProvider COMPONENT_EXPORT(COLOR)
    CreateEmulatedForcedColorsColorProvider(bool dark_mode);

// Fluent scrollbars have three main colors. This function completes the
// definition of colors for all scrollbar parts in relation to the three main
// ones.
void COMPONENT_EXPORT(COLOR)
    CompleteFluentScrollbarColorsDefinition(ui::ColorMixer& mixer);

// Returns true if `color_provider` and `renderer_color_map` map renderer
// color ids to the same SkColor.
bool COMPONENT_EXPORT(COLOR) IsRendererColorMappingEquivalent(
    const ColorProvider& color_provider,
    const RendererColorMap& renderer_color_map);

// Sets the callback for converting a ChromeColorId to a string name. This is
// used by ColorIdName. Only one callback is allowed.
void COMPONENT_EXPORT(COLOR)
    SetColorProviderUtilsCallbacks(ColorProviderUtilsCallbacks* callbacks);

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_UTILS_H_
