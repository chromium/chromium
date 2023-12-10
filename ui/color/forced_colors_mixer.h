// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_FORCED_COLORS_MIXER_H_
#define UI_COLOR_FORCED_COLORS_MIXER_H_

namespace ui {

class ColorMixer;
class ColorProvider;
struct ColorProviderKey;

// Adds the default system colors to `mixer` when forced colors is enabled on
// the OS.
void AddSystemForcedColorsToMixer(ColorMixer& mixer);

// Adds a color mixer to `provider` for forced colors mode.
void AddForcedColorsColorMixer(ColorProvider* provider,
                               const ColorProviderKey& key);

}  // namespace ui

#endif  // UI_COLOR_FORCED_COLORS_MIXER_H_
