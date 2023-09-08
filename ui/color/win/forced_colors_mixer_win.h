// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_FORCED_COLORS_MIXER_WIN_H_
#define UI_COLOR_FORCED_COLORS_MIXER_WIN_H_

namespace ui {

class ColorProvider;
struct ColorProviderKey;

// Adds a color mixer to `provider` that applies the default Windows system
// colors when High contrast mode is enabled.
void AddSystemForcedColorsColorMixer(ColorProvider* provider,
                                     const ColorProviderKey& key);

}  // namespace ui

#endif  // UI_COLOR_FORCED_COLORS_MIXER_WIN_H_
