// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#import <Cocoa/Cocoa.h>

#include "base/containers/fixed_flat_set.h"
#import "skia/ext/skia_utils_mac.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_set.h"
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
                             bool dark_window,
                             bool high_contrast) {
  ScopedCurrentNSAppearance scoped_nsappearance(dark_window, high_contrast);
  ColorMixer& mixer = provider->AddMixer();
  mixer.AddSet({kColorSetNative,
                {
                    {kColorItemHighlight,
                     SkColorSetA(skia::NSSystemColorToSkColor(
                                     [NSColor keyboardFocusIndicatorColor]),
                                 0x66)},
                    {kColorTextSelectionBackground,
                     skia::NSSystemColorToSkColor(
                         [NSColor selectedTextBackgroundColor])},
                }});
}

void AddNativeColorSetInColorMixer(ColorMixer& mixer) {
  mixer.AddSet(
      {kColorSetNative,
       {
           {kColorMenuBorder, SkColorSetA(SK_ColorBLACK, 0x60)},
           {kColorMenuItemForegroundDisabled,
            skia::NSSystemColorToSkColor([NSColor disabledControlTextColor])},
           {kColorMenuItemForeground,
            skia::NSSystemColorToSkColor([NSColor controlTextColor])},
       }});
}

void AddNativeUiColorMixer(ColorProvider* provider,
                           bool dark_window,
                           bool high_contrast) {
  ScopedCurrentNSAppearance scoped_nsappearance(dark_window, high_contrast);
  ColorMixer& mixer = provider->AddMixer();

  // TODO(crbug.com/1268521): Investigate native color set behaviour for dark
  // windows on macOS versions running < 10.14.
  if (@available(macOS 10.14, *)) {
    AddNativeColorSetInColorMixer(mixer);
  } else if (!dark_window) {
    AddNativeColorSetInColorMixer(mixer);
  }

  if (@available(macOS 10.14, *)) {
    mixer[kColorTableBackgroundAlternate] = {skia::NSSystemColorToSkColor(
        NSColor.alternatingContentBackgroundColors[1])};
  } else {
    mixer[kColorTableBackgroundAlternate] = {skia::NSSystemColorToSkColor(
        NSColor.controlAlternatingRowBackgroundColors[1])};
  }

  SkColor menu_separator_color = dark_window
                                     ? SkColorSetA(gfx::kGoogleGrey800, 0xCC)
                                     : SkColorSetA(SK_ColorBLACK, 0x26);
  mixer[kColorMenuSeparator] = {menu_separator_color};

  if (!high_contrast)
    return;

  if (dark_window) {
    mixer[kColorMenuItemForegroundSelected] = {SK_ColorBLACK};
    mixer[kColorMenuItemBackgroundSelected] = {SK_ColorLTGRAY};
  } else {
    mixer[kColorMenuItemForegroundSelected] = {SK_ColorWHITE};
    mixer[kColorMenuItemBackgroundSelected] = {SK_ColorDKGRAY};
  }
}

void AddNativePostprocessingMixer(ColorProvider* provider) {
  ColorMixer& mixer = provider->AddPostprocessingMixer();

  for (ColorId id = kUiColorsStart; id < kUiColorsEnd; ++id) {
    // Apply system tint to non-OS colors.
    if (!kNativeOSColorIds.contains(id))
      mixer[id] += ApplySystemControlTintIfNeeded();
  }
}

}  // namespace ui
