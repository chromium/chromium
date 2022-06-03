// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/native_color_mixers.h"

#include "build/build_config.h"

namespace ui {

#if !defined(OS_MAC) && !defined(OS_WIN)
void AddNativeCoreColorMixer(ColorProvider* provider,
                             bool dark_window,
                             bool high_contrast) {}

void AddNativeUiColorMixer(ColorProvider* provider,
                           bool dark_window,
                           bool high_contrast) {}
#endif

#if !defined(OS_MAC)
void AddNativePostprocessingMixer(ColorProvider* provider) {}
#endif

}  // namespace ui
