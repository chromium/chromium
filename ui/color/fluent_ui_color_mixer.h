// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_FLUENT_UI_COLOR_MIXER_H_
#define UI_COLOR_FLUENT_UI_COLOR_MIXER_H_

namespace ui {

class ColorProvider;
struct ColorProviderKey;

// Adds a color mixer to `provider` for fluent mappings.
void AddFluentUiColorMixer(ColorProvider* provider,
                           const ColorProviderKey& key);

}  // namespace ui

#endif  // UI_COLOR_FLUENT_UI_COLOR_MIXER_H_
