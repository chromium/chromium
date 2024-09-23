// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_UTILS_H_
#define UI_COLOR_COLOR_PROVIDER_UTILS_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
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
  virtual bool ColorIdName(ColorId color_id, std::string_view* color_name) = 0;
};

// The following functions convert various values to strings intended for
// logging. Do not retain the results for longer than the scope in which these
// functions are called.

// Converts the ColorMode.
std::string_view COMPONENT_EXPORT(COLOR)
    ColorModeName(ColorProviderKey::ColorMode color_mode);

// Converts the ContrastMode.
std::string_view COMPONENT_EXPORT(COLOR)
    ContrastModeName(ColorProviderKey::ContrastMode contrast_mode);

// Converts the ForcedColors.
std::string_view COMPONENT_EXPORT(COLOR)
    ForcedColorsName(ColorProviderKey::ForcedColors forced_colors);

// Converts SystemTheme.
std::string_view COMPONENT_EXPORT(COLOR)
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
std::unique_ptr<ColorProvider> COMPONENT_EXPORT(COLOR)
    CreateColorProviderFromRendererColorMap(
        const RendererColorMap& renderer_color_map);

// Adds colors for emulating Windows 10 default high contrast color themes
// to `mixer`. Used to support the devtools forced colors emulation feature.
void COMPONENT_EXPORT(COLOR)
    AddEmulatedForcedColorsToMixer(ColorMixer& mixer, bool dark_mode);

// Creates a color provider emulating Windows 10 default high contrast color
// themes.
std::unique_ptr<ColorProvider> COMPONENT_EXPORT(COLOR)
    CreateEmulatedForcedColorsColorProvider(bool dark_mode);

// TODO(samomekarajr): Forced colors web tests currently rely on specific set of
// hardcoded colors for for determining which system colors to render. This
// function should be updated once the web driver support spec for forced colors
// mode is updated.
std::unique_ptr<ColorProvider> COMPONENT_EXPORT(COLOR)
    CreateEmulatedForcedColorsColorProviderForTest();

// TODO(crbug.com/40779801): Enhance this function by incorporating platform
// specific overrides, particularly for CSS system colors.
// Creates a default fallback color provider for Blink Pages that are not
// associated with a web view. This includes tests, dummy pages, and non
// ordinary pages. These scenarios do not use the normal machinery to establish
// color providers in the renderer. The color mappings for this provider are
// derived from old Aura colors for controls.
std::unique_ptr<ColorProvider> COMPONENT_EXPORT(COLOR)
    CreateDefaultColorProviderForBlink(bool dark_mode);

// Scrollbars have three main colors. This function completes the
// definition of colors for all scrollbar parts in relation to the three main
// ones.
void COMPONENT_EXPORT(COLOR)
    CompleteScrollbarColorsDefinition(ui::ColorMixer& mixer);

// Completes color definitions for the controls defined in
// NativeThemeBase::ControlColorId when in forced colors mode.
void COMPONENT_EXPORT(COLOR)
    CompleteControlsForcedColorsDefinition(ui::ColorMixer& mixer);

// Completes default color definitions for the RendererColorIds that are non
// web native.
void COMPONENT_EXPORT(COLOR)
    CompleteDefaultNonWebNativeRendererColorIdsDefinition(
        ui::ColorMixer& mixer);

// Completes default color definitions for the CSS system colors.
void COMPONENT_EXPORT(COLOR)
    CompleteDefaultCssSystemColorDefinition(ui::ColorMixer& mixer,
                                            bool dark_mode);

// Returns a default set of color maps for tests and non ordinary pages. These
// places do not use the normal machinery to establish a color provider in the
// renderer since they are not associated with a web view.
RendererColorMap COMPONENT_EXPORT(COLOR)
    GetDefaultBlinkColorProviderColorMaps(bool dark_mode,
                                          bool is_forced_colors);

// Returns true if `color_provider` and `renderer_color_map` map renderer
// color ids to the same SkColor.
bool COMPONENT_EXPORT(COLOR) IsRendererColorMappingEquivalent(
    const ColorProvider* color_provider,
    const RendererColorMap& renderer_color_map);

// Sets the callback for converting a ChromeColorId to a string name. This is
// used by ColorIdName. Only one callback is allowed.
void COMPONENT_EXPORT(COLOR)
    SetColorProviderUtilsCallbacks(ColorProviderUtilsCallbacks* callbacks);

}  // namespace ui

#endif  // UI_COLOR_COLOR_PROVIDER_UTILS_H_
