// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#import <Cocoa/Cocoa.h>

#include "base/containers/fixed_flat_set.h"
#import "skia/ext/skia_utils_mac.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_recipe.h"
#include "ui/gfx/color_palette.h"

namespace ui {

namespace {

// All the native OS colors which are retrieved from the system directly.
constexpr auto kNativeOSColorIds = base::MakeFixedFlatSet<ColorId>({
    // clang-format off
    kColorFocusableBorderFocused,
    kColorLabelSelectionBackground,
    kColorMenuBorder,
    kColorMenuItemForegroundDisabled,
    kColorMenuItemForeground,
    kColorMenuSeparator,
    kColorTableBackgroundAlternate,
    kColorTableGroupingIndicator,
    kColorTextfieldSelectionBackground
    // clang-format on
});

struct AppearanceProperties {
  bool dark;
  bool high_contrast;
};

AppearanceProperties AppearancePropertiesForKey(const ColorProviderKey& key) {
  return AppearanceProperties{
      .dark = key.color_mode == ColorProviderKey::ColorMode::kDark,
      .high_contrast =
          key.contrast_mode == ColorProviderKey::ContrastMode::kHigh};
}

NSAppearance* AppearanceForKey(const ColorProviderKey& key) {
  AppearanceProperties properties = AppearancePropertiesForKey(key);

  // TODO(crbug.com/40258902): How does this work? The documentation says that
  // the high contrast appearance names are not valid to pass to `-[NSAppearance
  // appearanceNamed:]` and yet this code does so. This yields the same
  // `NSAppearance` objects that result from passing the non-high contrast names
  // to -`appearanceNamed:`.
  if (properties.dark) {
    return [NSAppearance
        appearanceNamed:properties.high_contrast
                            ? NSAppearanceNameAccessibilityHighContrastDarkAqua
                            : NSAppearanceNameDarkAqua];
  } else {
    return [NSAppearance
        appearanceNamed:properties.high_contrast
                            ? NSAppearanceNameAccessibilityHighContrastAqua
                            : NSAppearanceNameAqua];
  }
}

}  // namespace

void AddNativeCoreColorMixer(ColorProvider* provider,
                             const ColorProviderKey& key) {
  auto load_colors = ^{
    ColorMixer& mixer = provider->AddMixer();
    mixer[kColorItemHighlight] = {SkColorSetA(
        skia::NSSystemColorToSkColor(NSColor.keyboardFocusIndicatorColor),
        0x66)};
  };

  [AppearanceForKey(key) performAsCurrentDrawingAppearance:load_colors];
}

void AddNativeColorSetInColorMixer(ColorMixer& mixer) {
  mixer[kColorMenuBorder] = {SkColorSetA(SK_ColorBLACK, 0x60)};
  mixer[kColorMenuItemForegroundDisabled] = {
      skia::NSSystemColorToSkColor(NSColor.disabledControlTextColor)};
  mixer[kColorMenuItemForeground] = {
      skia::NSSystemColorToSkColor(NSColor.controlTextColor)};
}

void AddNativeUiColorMixer(ColorProvider* provider,
                           const ColorProviderKey& key) {
  auto load_colors = ^{
    AppearanceProperties properties = AppearancePropertiesForKey(key);

    ColorMixer& mixer = provider->AddMixer();

    AddNativeColorSetInColorMixer(mixer);

    mixer[kColorTableBackgroundAlternate] = {skia::NSSystemColorToSkColor(
        NSColor.alternatingContentBackgroundColors[1])};
    if (!key.user_color.has_value()) {
      mixer[kColorSysStateFocusRing] = PickGoogleColor(
          skia::NSSystemColorToSkColor(NSColor.keyboardFocusIndicatorColor),
          kColorSysBase, color_utils::kMinimumVisibleContrastRatio);

      const SkColor system_highlight_color =
          skia::NSSystemColorToSkColor(NSColor.selectedTextBackgroundColor);
      mixer[kColorTextSelectionBackground] = {system_highlight_color};

      // TODO(crbug.com/40074489): Address accessibility for mac highlight
      // colors.
      mixer[kColorSysStateTextHighlight] = {system_highlight_color};
      mixer[kColorSysStateOnTextHighlight] = {kColorSysOnSurface};
    }

    if (!properties.high_contrast) {
      return;
    }

    mixer[kColorMenuItemBackgroundSelected] = {
        properties.dark ? SK_ColorLTGRAY : SK_ColorDKGRAY};
    mixer[kColorMenuItemForegroundSelected] = {properties.dark ? SK_ColorBLACK
                                                               : SK_ColorWHITE};
  };

  [AppearanceForKey(key) performAsCurrentDrawingAppearance:load_colors];
}

void AddNativePostprocessingMixer(ColorProvider* provider,
                                  const ColorProviderKey& key) {
  // Ensure the system tint is applied by default for pre-refresh browsers. For
  // post-refresh only apply the tint if running old design system themes or the
  // color source is explicitly configured for grayscale.
  if (!key.custom_theme &&
      key.user_color_source != ColorProviderKey::UserColorSource::kGrayscale) {
    return;
  }

  ColorMixer& mixer = provider->AddPostprocessingMixer();

  for (ColorId id = kUiColorsStart; id < kUiColorsEnd; ++id) {
    // Apply system tint to non-OS colors.
    if (!kNativeOSColorIds.contains(id))
      mixer[id] += ApplySystemControlTintIfNeeded();
  }
}

}  // namespace ui
