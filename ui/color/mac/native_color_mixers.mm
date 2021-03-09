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
    ui::kColorTextSelectionBackground,
    ui::kColorTextfieldSelectionBackground,
    ui::kColorTableBackgroundAlternate});
// clang-format on
}

namespace ui {

void AddNativeCoreColorMixer(ColorProvider* provider,
                             bool dark_window,
                             bool high_contrast) {
  ScopedCurrentNSAppearance scoped_nsappearance(dark_window);
  ColorMixer& mixer = provider->AddMixer();
  mixer.AddSet({kColorSetNative,
                {
                    {kColorTextSelectionBackground,
                     skia::NSSystemColorToSkColor(
                         [NSColor selectedTextBackgroundColor])},
                }});
}

void AddNativeUiColorMixer(ColorProvider* provider,
                           bool dark_window,
                           bool high_contrast) {
  ScopedCurrentNSAppearance scoped_nsappearance(dark_window);
  ColorMixer& mixer = provider->AddMixer();
  mixer.AddSet(
      {kColorSetNative,
       {
           {kColorFocusableBorderFocused,
            SkColorSetA(skia::NSSystemColorToSkColor(
                            [NSColor keyboardFocusIndicatorColor]),
                        0x66)},
           {kColorMenuBorder, SkColorSetA(SK_ColorBLACK, 0x60)},
           {kColorMenuItemForegroundDisabled,
            skia::NSSystemColorToSkColor([NSColor disabledControlTextColor])},
           {kColorMenuItemForeground,
            skia::NSSystemColorToSkColor([NSColor controlTextColor])},
           {kColorTextSelectionBackground,
            skia::NSSystemColorToSkColor(
                [NSColor selectedTextBackgroundColor])},
       }});

  mixer[kColorMenuItemForegroundHighlighted] = {kColorPrimaryForeground};
  mixer[kColorMenuItemForegroundSelected] = {kColorPrimaryForeground};

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
}

void AddSystemTintMixer(ColorProvider* provider) {
  ColorMixer& mixer = provider->AddMixer();

  for (ui::ColorId id = ui::kUiColorsStart; id < kUiColorsLast; ++id) {
    // Apply system tint to non-OS colors.
    if (!kNativeOSColorIds.contains(id))
      mixer[id] += ApplySystemControlTintIfNeeded();
  }
}

}  // namespace ui
