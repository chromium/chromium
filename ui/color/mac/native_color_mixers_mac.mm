// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>

#include "base/check_op.h"
#include "base/containers/fixed_flat_set.h"
#import "skia/ext/skia_utils_mac.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_mixers.h"
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

SkColor NSSystemColorToSkColor(NSColor* color) {
  // It is expected that the colors that will flow through this function will be
  // catalog colors. Being a catalog color means that it doesn't have explicit
  // components, so convert it to a color that has components. If that resulting
  // color can be converted, then we're done here.
  NSColor* device_color =
      [color colorUsingColorSpace:NSColorSpace.deviceRGBColorSpace];
  if (device_color) {
    return skia::NSDeviceColorToSkColor(device_color);
  }

  // Sometimes the conversion is not possible, but we can get an approximation
  // by going through a CGColorRef. TODO: Is this still the case? What is the
  // situation where converting as an NSColor fails but converting as a
  // CGColorRef succeeds?
  CGColorRef cg_color = color.CGColor;
  size_t component_count = CGColorGetNumberOfComponents(cg_color);

  // 4 components means RGBA.
  if (component_count == 4) {
    return skia::CGColorRefToSkColor(cg_color);
  }

  // 1-2 components means a grayscale channel and maybe an alpha channel, which
  // CGColorRefToSkColor will not like. But RGB is additive, so the conversion
  // is easy (RGB to grayscale is less easy).
  CHECK(component_count == 1 || component_count == 2);
  float gray_value = *CGColorGetComponents(cg_color);
  float alpha_value = CGColorGetAlpha(cg_color);

  return SkColor4f{gray_value, gray_value, gray_value, alpha_value}.toSkColor();
}

}  // namespace

void AddNativeCoreColorMixer(ColorProvider* provider,
                             const ColorProviderKey& key) {
  auto load_colors = ^{
    ColorMixer& mixer = provider->AddMixer();
    mixer[kColorItemHighlight] = {SkColorSetA(
        NSSystemColorToSkColor(NSColor.keyboardFocusIndicatorColor), 0x66)};
  };

  [AppearanceForKey(key) performAsCurrentDrawingAppearance:load_colors];
}

void AddNativeColorSetInColorMixer(ColorMixer& mixer) {
  mixer[kColorMenuBorder] = {SkColorSetA(SK_ColorBLACK, 0x60)};
  mixer[kColorMenuItemForegroundDisabled] = {
      NSSystemColorToSkColor(NSColor.disabledControlTextColor)};
  mixer[kColorMenuItemForeground] = {
      NSSystemColorToSkColor(NSColor.controlTextColor)};
}

void AddNativeUiColorMixer(ColorProvider* provider,
                           const ColorProviderKey& key) {
  auto load_colors = ^{
    AppearanceProperties properties = AppearancePropertiesForKey(key);

    ColorMixer& mixer = provider->AddMixer();

    AddNativeColorSetInColorMixer(mixer);

    mixer[kColorTableBackgroundAlternate] = {
        NSSystemColorToSkColor(NSColor.alternatingContentBackgroundColors[1])};
    if (!key.user_color.has_value()) {
      mixer[kColorSysStateFocusRing] = PickGoogleColor(
          NSSystemColorToSkColor(NSColor.keyboardFocusIndicatorColor),
          kColorSysBase, color_utils::kMinimumVisibleContrastRatio);

      const SkColor system_highlight_color =
          NSSystemColorToSkColor(NSColor.selectedTextBackgroundColor);
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

    mixer[kColorTableRowHighlight] = {kColorSysStateHoverOnSubtle};
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
    if (!kNativeOSColorIds.contains(id)) {
      mixer[id] += ApplySystemControlTintIfNeeded();
    }
  }
}

}  // namespace ui
