// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_NATIVE_COLOR_MIXERS_H_
#define UI_COLOR_NATIVE_COLOR_MIXERS_H_

namespace ui {

class ColorProvider;

// Adds a color mixer to |provider| that provide kColorSetNative.
// This function should be implemented on a per-platform basis in
// relevant subdirectories.
void AddNativeCoreColorMixer(ColorProvider* provider,
                             bool dark_window,
                             bool high_contrast);

// Adds a color mixer to |provider| that can add to kColorSetNative.
// Intended for colors needed by ui/ that this platform overrides but
// are outside the set defined in the core mixer.
void AddNativeUiColorMixer(ColorProvider* provider,
                           bool dark_window,
                           bool high_contrast);

void AddNativePostprocessingMixer(ColorProvider* provider);

}  // namespace ui

#endif  // UI_COLOR_NATIVE_COLOR_MIXERS_H_
