// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_NATIVE_COLOR_MIXERS_H_
#define UI_COLOR_NATIVE_COLOR_MIXERS_H_

#include "ui/color/color_provider_key.h"

namespace ui {

class ColorProvider;

// Adds a color mixer to |provider| that provide kColorSetNative.
// This function should be implemented on a per-platform basis in
// relevant subdirectories.
void AddNativeCoreColorMixer(ColorProvider* provider,
                             const ColorProviderKey& key);

// Adds a color mixer to |provider| that can add to kColorSetNative.
// Intended for colors needed by ui/ that this platform overrides but
// are outside the set defined in the core mixer.
void AddNativeUiColorMixer(ColorProvider* provider,
                           const ColorProviderKey& key);

void AddNativePostprocessingMixer(ColorProvider* provider,
                                  const ColorProviderKey& key);

}  // namespace ui

#endif  // UI_COLOR_NATIVE_COLOR_MIXERS_H_
