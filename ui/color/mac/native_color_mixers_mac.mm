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
#include "ui/color/mac/scoped_current_nsappearance.h"
#include "ui/gfx/color_palette.h"

namespace {
// All the native OS colors which are retrieved from the system directly.
// clang-format off
constexpr auto kNativeOSColorIds = base::MakeFixedFlatSet<ui::ColorId>({
    ui::kColorFocusableBorderFocused,
    ui::kColorLabelSelectionBackground,
    ui::kColorMenuBorder,
    ui::kColorMenuItemForegroundDisabled,
    ui::kColorMenuItemForeground,
    ui::kColorMenuSeparator,
    ui::kColorTableBackgroundAlternate,
    ui::kColorTableGroupingIndicator,
    ui::kColorTextfieldSelectionBackground});
// clang-format on
}

namespace ui {

void AddNativeCoreColorMixer(ColorProvider* provider,
                             const ColorProviderManager::Key& key) {
  ScopedCurrentNSAppearance scoped_nsappearance(
      key.color_mode == ColorProviderManager::ColorMode::kDark,
      key.contrast_mode == ColorProviderManager::ContrastMode::kHigh);
  ColorMixer& mixer = provider->AddMixer();
  mixer[kColorItemHighlight] = {SkColorSetA(
      skia::NSSystemColorToSkColor([NSColor keyboardFocusIndicatorColor]),
      0x66)};
  mixer[kColorTextSelectionBackground] = {
      skia::NSSystemColorToSkColor([NSColor selectedTextBackgroundColor])};
}

void AddNativeColorSetInColorMixer(ColorMixer& mixer) {
  mixer[kColorMenuBorder] = {SkColorSetA(SK_ColorBLACK, 0x60)};
  mixer[kColorMenuItemForegroundDisabled] = {
      skia::NSSystemColorToSkColor([NSColor disabledControlTextColor])};
  mixer[kColorMenuItemForeground] = {
      skia::NSSystemColorToSkColor([NSColor controlTextColor])};
}

void AddNativeUiColorMixer(ColorProvider* provider,
                           const ColorProviderManager::Key& key) {
  const bool dark_mode =
      key.color_mode == ColorProviderManager::ColorMode::kDark;
  const bool high_contrast =
      key.contrast_mode == ColorProviderManager::ContrastMode::kHigh;
  ScopedCurrentNSAppearance scoped_nsappearance(dark_mode, high_contrast);
  ColorMixer& mixer = provider->AddMixer();

  // TODO(crbug.com/1268521): Investigate native color set behaviour for dark
  // windows on macOS versions running < 10.14.
  if (@available(macOS 10.14, *)) {
    AddNativeColorSetInColorMixer(mixer);
  } else if (!dark_mode) {
    AddNativeColorSetInColorMixer(mixer);
  }

  if (@available(macOS 10.14, *)) {
    mixer[kColorTableBackgroundAlternate] = {skia::NSSystemColorToSkColor(
        NSColor.alternatingContentBackgroundColors[1])};
  } else {
    mixer[kColorTableBackgroundAlternate] = {skia::NSSystemColorToSkColor(
        NSColor.controlAlternatingRowBackgroundColors[1])};
  }

  SkColor menu_separator_color = dark_mode
                                     ? SkColorSetA(gfx::kGoogleGrey800, 0xCC)
                                     : SkColorSetA(SK_ColorBLACK, 0x26);
  mixer[kColorMenuSeparator] = {menu_separator_color};

  if (!high_contrast)
    return;

  mixer[kColorMenuItemBackgroundSelected] = {dark_mode ? SK_ColorLTGRAY
                                                       : SK_ColorDKGRAY};
  mixer[kColorMenuItemForegroundSelected] = {dark_mode ? SK_ColorBLACK
                                                       : SK_ColorWHITE};
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
