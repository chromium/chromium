// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_REF_COLOR_MIXER_H_
#define UI_COLOR_REF_COLOR_MIXER_H_

#include "ui/color/color_provider_manager.h"

namespace ui {

class ColorProvider;

// Adds a color mixer to `provider` that provides the reference color tokens for
// Google Material 3.
void AddRefColorMixer(ColorProvider* provider,
                      const ColorProviderManager::Key& key);

}  // namespace ui

#endif  // UI_COLOR_REF_COLOR_MIXER_H_
