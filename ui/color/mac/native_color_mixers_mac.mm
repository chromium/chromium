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
#include "ui/color/color_provider_manager.h"
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

AppearanceProperties AppearancePropertiesForKey(
    const ColorProviderManager::Key& key) {
  return AppearanceProperties{
      .dark = key.color_mode == ColorProviderManager::ColorMode::kDark,
      .high_contrast =
          key.contrast_mode == ColorProviderManager::ContrastMode::kHigh};
}

NSAppearance* AppearanceForKey(const ColorProviderManager::Key& key)
    API_AVAILABLE(macos(10.14)) {
  AppearanceProperties properties = AppearancePropertiesForKey(key);

  // TODO(crbug.com/1420707): How does this work? The documentation says that
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
                             const ColorProviderManager::Key& key) {
  auto load_colors = ^{
    ColorMixer& mixer = provider->AddMixer();
    mixer[kColorItemHighlight] = {SkColorSetA(
        skia::NSSystemColorToSkColor(NSColor.keyboardFocusIndicatorColor),
        0x66)};
    mixer[kColorTextSelectionBackground] = {
        skia::NSSystemColorToSkColor(NSColor.selectedTextBackgroundColor)};
  };

  if (@available(macOS 11, *)) {
    [AppearanceForKey(key) performAsCurrentDrawingAppearance:load_colors];
  } else if (@available(macOS 10.14, *)) {
    NSAppearance* saved_appearance = NSAppearance.currentAppearance;
    NSAppearance.currentAppearance = AppearanceForKey(key);
    load_colors();
    NSAppearance.currentAppearance = saved_appearance;
  } else {
    load_colors();
  }
}

void AddNativeColorSetInColorMixer(ColorMixer& mixer) {
  mixer[kColorMenuBorder] = {SkColorSetA(SK_ColorBLACK, 0x60)};
  mixer[kColorMenuItemForegroundDisabled] = {
      skia::NSSystemColorToSkColor(NSColor.disabledControlTextColor)};
  mixer[kColorMenuItemForeground] = {
      skia::NSSystemColorToSkColor(NSColor.controlTextColor)};
}

void AddNativeUiColorMixer(ColorProvider* provider,
                           const ColorProviderManager::Key& key) {
  auto load_colors = ^{
    AppearanceProperties properties = AppearancePropertiesForKey(key);

    ColorMixer& mixer = provider->AddMixer();

    // TODO(crbug.com/1268521): Investigate native color set behaviour for dark
    // windows on macOS versions running < 10.14.
    if (@available(macOS 10.14, *)) {
      AddNativeColorSetInColorMixer(mixer);
    } else if (!properties.dark) {
      AddNativeColorSetInColorMixer(mixer);
    }

    if (@available(macOS 10.14, *)) {
      mixer[kColorTableBackgroundAlternate] = {skia::NSSystemColorToSkColor(
          NSColor.alternatingContentBackgroundColors[1])};
    } else {
      mixer[kColorTableBackgroundAlternate] = {skia::NSSystemColorToSkColor(
          NSColor.controlAlternatingRowBackgroundColors[1])};
    }

    SkColor menu_separator_color = properties.dark
                                       ? SkColorSetA(gfx::kGoogleGrey800, 0xCC)
                                       : SkColorSetA(SK_ColorBLACK, 0x26);
    mixer[kColorMenuSeparator] = {menu_separator_color};

    if (!properties.high_contrast) {
      return;
    }

    mixer[kColorMenuItemBackgroundSelected] = {
        properties.dark ? SK_ColorLTGRAY : SK_ColorDKGRAY};
    mixer[kColorMenuItemForegroundSelected] = {properties.dark ? SK_ColorBLACK
                                                               : SK_ColorWHITE};
  };

  if (@available(macOS 11, *)) {
    [AppearanceForKey(key) performAsCurrentDrawingAppearance:load_colors];
  } else if (@available(macOS 10.14, *)) {
    NSAppearance* saved_appearance = NSAppearance.currentAppearance;
    NSAppearance.currentAppearance = AppearanceForKey(key);
    load_colors();
    NSAppearance.currentAppearance = saved_appearance;
  } else {
    load_colors();
  }
}

void AddNativePostprocessingMixer(ColorProvider* provider,
                                  const ColorProviderManager::Key& key) {
  ColorMixer& mixer = provider->AddPostprocessingMixer();

  for (ColorId id = kUiColorsStart; id < kUiColorsEnd; ++id) {
    // Apply system tint to non-OS colors.
    if (!kNativeOSColorIds.contains(id))
      mixer[id] += ApplySystemControlTintIfNeeded();
  }
}

}  // namespace ui
