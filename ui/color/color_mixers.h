// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_MIXERS_H_
#define UI_COLOR_COLOR_MIXERS_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace ui {

class ColorProvider;

// The ordering of the mixer functions below reflects the
// order in which they are added to the providers.

// Adds a color mixer to |provider| that provide kColorSetCoreDefaults.
// |dark window| should be set if the window for this provider is "dark themed",
// e.g. system native dark mode is enabled or the window is incognito.
COMPONENT_EXPORT(COLOR)
void AddCoreDefaultColorMixer(ColorProvider* provider,
                              bool dark_window,
                              bool high_contrast);

// Adds a color mixer to |provider| that provide kColorSetNative.
// This function should be implemented on a per-platform basis in
// relevant subdirectories.
COMPONENT_EXPORT(COLOR)
void AddNativeCoreColorMixer(ColorProvider* provider,
                             bool dark_window,
                             bool high_contrast);

// Adds a color mixer to |provider| that combine the above color sets with
// recipes as necessary to produce all colors needed by ui/.
COMPONENT_EXPORT(COLOR)
void AddUiColorMixer(ColorProvider* provider,
                     bool dark_window,
                     bool high_contrast);

// Adds a color mixer to |provider| that can add to kColorSetNative.
// Intended for colors needed by ui/ that this platform overrides but
// are outside the set defined in the core mixer.
COMPONENT_EXPORT(COLOR)
void AddNativeUiColorMixer(ColorProvider* provider,
                           bool dark_window,
                           bool high_contrast);

COMPONENT_EXPORT(COLOR)
void AddNativePostprocessingMixer(ColorProvider* provider);

}  // namespace ui

#endif  // UI_COLOR_COLOR_MIXERS_H_
