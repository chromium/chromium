// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_CSS_SYSTEM_COLOR_MIXER_H_
#define UI_COLOR_CSS_SYSTEM_COLOR_MIXER_H_

namespace ui {

class ColorMixer;
class ColorProvider;
struct ColorProviderKey;

void MapNativeColorsToCssSystemColors(ColorMixer& mixer, ColorProviderKey key);

// Adds a color mixer to `provider` for system colors. Intended
// for colors used by Blink to support CSS system colors.
void AddCssSystemColorMixer(ColorProvider* provider,
                            const ColorProviderKey& key);

}  // namespace ui

#endif  // UI_COLOR_CSS_SYSTEM_COLOR_MIXER_H_
