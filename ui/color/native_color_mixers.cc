// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/native_color_mixers.h"

#include "build/build_config.h"
#include "ui/color/color_provider_key.h"

namespace ui {

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
void AddNativeCoreColorMixer(ColorProvider* provider,
                             const ColorProviderKey& key) {}
#endif

#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN)
void AddNativeUiColorMixer(ColorProvider* provider,
                           const ColorProviderKey& key) {}
#endif

#if !BUILDFLAG(IS_MAC)
void AddNativePostprocessingMixer(ColorProvider* provider,
                                  const ColorProviderKey& key) {}
#endif

}  // namespace ui
