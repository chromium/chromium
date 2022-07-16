// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_CORE_DEFAULT_COLOR_MIXER_H_
#define UI_COLOR_CORE_DEFAULT_COLOR_MIXER_H_

namespace ui {

class ColorProvider;

// Adds a color mixer to |provider| that provide kColorSetCoreDefaults.
// |dark window| should be set if the window for this provider is "dark themed",
// e.g. system native dark mode is enabled or the window is incognito.
void AddCoreDefaultColorMixer(ColorProvider* provider,
                              bool dark_window,
                              bool high_contrast);

}  // namespace ui

#endif  // UI_COLOR_CORE_DEFAULT_COLOR_MIXER_H_