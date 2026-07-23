// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_ANDROID_SYS_COLOR_MIXER_ANDROID_H_
#define UI_COLOR_ANDROID_SYS_COLOR_MIXER_ANDROID_H_

#include "base/component_export.h"
#include "ui/color/color_provider_key.h"

namespace ui {

class ColorProvider;

COMPONENT_EXPORT(COLOR)
void AddSysColorMixerAndroid(ColorProvider* provider,
                             const ColorProviderKey& key);

}  // namespace ui

#endif  // UI_COLOR_ANDROID_SYS_COLOR_MIXER_ANDROID_H_
