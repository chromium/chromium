// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_PROVIDER_UTILS_H_
#define UI_COLOR_COLOR_PROVIDER_UTILS_H_

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_id.h"
#include "ui/color/color_id.mojom-forward.h"
#include "ui/color/color_provider_manager.h"

namespace ui {

namespace internal {

// Compile-time string transformation that converts a ColorId enum string
// literal (e.g. "kColorSysPrimary") into its kebab-case CSS variable name
// representation (e.g. "--color-sys-primary").
//
// The output buffer is guaranteed to fit within `2 * N + 1` characters, as the
// initial 'k' is replaced by a leading '-', each subsequent uppercase character
// can expand to at most two characters ('-' followed by the lowercase ASCII
// character), and one extra slot is reserved for null-termination.
template <size_t N>
struct ConstexprCssColorName {
  std::array<char, 2 * N + 1> data{};
  size_t length = 0;

  constexpr explicit ConstexprCssColorName(const char (&str)[N]) {
    if (N <= 1) {
      return;
    }
    // SAFETY: The output buffer `data` has size 2 * N + 1, and each input char
    // in `str` produces at most 2 output chars plus the leading '-' and '\0'.
    UNSAFE_BUFFERS({
      data[length++] = '-';
      for (size_t i = 1; i < N - 1; ++i) {
        char c = str[i];
        if (c >= 'A' && c <= 'Z') {
          data[length++] = '-';
          data[length++] = static_cast<char>(c + ('a' - 'A'));
        } else {
          data[length++] = c;
        }
      }
      data[length] = '\0';
    });
  }

  constexpr std::string_view view() const {
    return std::string_view(data.data(), length);
  }
};

// Structural non-type template parameter (NTTP) holder for
// `ConstexprCssColorName`. Instantiating this template for a given compile-time
// name grants it static storage duration within the compilation unit, allowing
// `view()` to return a persistent `std::string_view` with zero runtime memory
// allocation or initialization cost.
template <ConstexprCssColorName Name>
struct CssNameHolder {
  static constexpr auto kName = Name;
};

}  // namespace internal

class ColorMixer;

using RendererColorMap = base::flat_map<color::mojom::RendererColorId, SkColor>;

class COMPONENT_EXPORT(COLOR) ColorProviderUtilsCallbacks {
 public:
  virtual ~ColorProviderUtilsCallbacks();
  virtual bool ColorIdName(ColorId color_id, std::string_view* color_name) = 0;
  virtual bool ColorIdToCSSColorId(ColorId color_id,
                                   std::string_view* css_name);
  virtual bool NameToColorId(std::string_view color_name, ColorId* color_id);
};

// The following functions convert various values to strings intended for
// logging. Do not retain the results for longer than the scope in which these
// functions are called.

// Converts ColorId.
std::string COMPONENT_EXPORT(COLOR) ColorIdName(ColorId color_id);

// Converts a `ColorId` to its CSS variable name format (e.g.
// "--color-primary-background") with zero runtime heap allocations.
//
// The mapping is backed by a compile-time `base::MakeFixedFlatMap` constructed
// via macro expansion and NTTP string storage, providing an O(log N) binary
// search that yields a persistent `std::string_view`. Falls back to embedder
// callbacks (such as `ChromeColorProviderUtils`) if the ID is not found in the
// core map.
std::string_view COMPONENT_EXPORT(COLOR) ColorIdToCSSColorId(ColorId color_id);

// Converts string representation of ColorId to its enum value.
std::optional<ColorId> COMPONENT_EXPORT(COLOR)
    NameToColorId(std::string_view color_id_name);

// Converts SkColor to string. Check if color matches a standard color palette
// value and return it as a string. Otherwise return as an rgba(xx, xxx, xxx,
// xxx) string.
std::string COMPONENT_EXPORT(COLOR) SkColorName(SkColor color);

// Converts Color Provider Color Id in string format from kColorXXX to
// "--color-X-X-X" for CSS
std::string COMPONENT_EXPORT(COLOR)
    ConvertColorProviderColorIdToCSSColorId(std::string_view color_id_name);

// Converts `color` to a CSS hex color string formatted as `#RRGGBBAA` and
// appends it directly into `out`.
//
// This avoids format string parsing (`base::StringPrintf`) and dynamic heap
// allocations by extracting ARGB nibbles directly and indexing into a static
// lookup table of hexadecimal digits.
void COMPONENT_EXPORT(COLOR)
    FastAppendCssHexColor(SkColor color, std::string& out);

// Formats `color` as comma-separated RGB values ("R,G,B") and appends it
// directly into `out`.
//
// Performs fast integer digit decomposition on each 8-bit channel (0-255) to
// emit ASCII characters directly, avoiding heap allocations or division loops.
void COMPONENT_EXPORT(COLOR)
    FastAppendRgbColor(SkColor color, std::string& out);

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

// Fluent scrollbars have three main colors. This function completes the
// definition of colors for all scrollbar parts in relation to the three main
// ones.
void COMPONENT_EXPORT(COLOR)
    CompleteFluentScrollbarColorsDefinition(ui::ColorMixer& mixer);

// Completes color definitions for the controls defined in
// NativeThemeBase::ControlColorId when in forced colors mode.
void COMPONENT_EXPORT(COLOR)
    CompleteControlsForcedColorsDefinition(ui::ColorMixer& mixer);

// Completes default color definitions for the RendererColorIds that are web
// native.
void COMPONENT_EXPORT(COLOR)
    CompleteDefaultWebNativeRendererColorIdsDefinition(ui::ColorMixer& mixer,
                                                       bool dark_mode,
                                                       bool high_contrast);

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
