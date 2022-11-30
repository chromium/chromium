// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include "ui/color/core_default_color_mixer.h"
#include "ui/color/native_color_mixers.h"
#include "ui/color/ref_color_mixer.h"
#include "ui/color/sys_color_mixer.h"
#include "ui/color/ui_color_mixer.h"

namespace ui {

void AddColorMixers(ColorProvider* provider,
                    const ColorProviderManager::Key& key) {
  AddRefColorMixer(provider, key);
  // TODO(tluk): Determine the correct place to insert the sys color mixer.
  AddSysColorMixer(provider, key);
  AddCoreDefaultColorMixer(provider, key);
  AddNativeCoreColorMixer(provider, key);
  AddUiColorMixer(provider, key);
  AddNativeUiColorMixer(provider, key);
  AddNativePostprocessingMixer(provider, key);
}

}  // namespace ui
