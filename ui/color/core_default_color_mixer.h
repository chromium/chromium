// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_CORE_DEFAULT_COLOR_MIXER_H_
#define UI_COLOR_CORE_DEFAULT_COLOR_MIXER_H_

#include "ui/color/color_provider_key.h"

namespace ui {

class ColorProvider;

// Adds a color mixer to |provider| that provides kColorSetCoreDefaults.
void AddCoreDefaultColorMixer(ColorProvider* provider,
                              const ColorProviderKey& key);

}  // namespace ui

#endif  // UI_COLOR_CORE_DEFAULT_COLOR_MIXER_H_
