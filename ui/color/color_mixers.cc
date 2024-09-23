// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_mixers.h"

#include "ui/base/ui_base_features.h"
#include "ui/color/core_default_color_mixer.h"
#include "ui/color/css_system_color_mixer.h"
#include "ui/color/fluent_ui_color_mixer.h"
#include "ui/color/material_ui_color_mixer.h"
#include "ui/color/native_color_mixers.h"
#include "ui/color/ref_color_mixer.h"
#include "ui/color/sys_color_mixer.h"
#include "ui/color/ui_color_mixer.h"
#include "ui/native_theme/native_theme_features.h"

namespace ui {

void AddColorMixers(ColorProvider* provider, const ColorProviderKey& key) {
  AddRefColorMixer(provider, key);
  // TODO(tluk): Determine the correct place to insert the sys color mixer.
  AddSysColorMixer(provider, key);
  AddCoreDefaultColorMixer(provider, key);
  AddNativeCoreColorMixer(provider, key);
  AddUiColorMixer(provider, key);
  AddMaterialUiColorMixer(provider, key);
  if (IsFluentScrollbarEnabled()) {
    // This must come after the UI and before the native UI mixers to ensure
    // leaf node colors are overridden with the Fluent recipes but that high
    // contrast (specified via native UI on Windows) can override the Fluent
    // colors.
    AddFluentUiColorMixer(provider, key);
  }
  AddNativeUiColorMixer(provider, key);
  AddCssSystemColorMixer(provider, key);
  AddNativePostprocessingMixer(provider, key);
}

}  // namespace ui
