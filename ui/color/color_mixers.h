// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_MIXERS_H_
#define UI_COLOR_COLOR_MIXERS_H_

#include "base/component_export.h"

namespace ui {

class ColorProvider;

// Adds color mixers to |provider| that provide kColorSetNative, as well as
// mappings from this set to cross-platform IDs.  This function should be
// implemented on a per-platform basis in relevant subdirectories.
COMPONENT_EXPORT(COLOR) void AddNativeColorMixers(ColorProvider* provider);

// Adds color mixers to |provider| that provide kColorSetCoreDefaults.
// |dark window| should be set if the window for this provider is "dark themed",
// e.g. system native dark mode is enabled or the window is incognito.
COMPONENT_EXPORT(COLOR)
void AddCoreDefaultColorMixers(ColorProvider* provider, bool dark_window);

// Adds color mixers to |provider| that combine the above color sets with
// recipes as necessary to produce all colors needed by ui/.
COMPONENT_EXPORT(COLOR) void AddUiColorMixers(ColorProvider* provider);

}  // namespace ui

#endif  // UI_COLOR_COLOR_MIXERS_H_
