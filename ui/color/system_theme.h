// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_SYSTEM_THEME_H_
#define UI_COLOR_SYSTEM_THEME_H_

#include "build/build_config.h"

namespace ui {

// Don't change the order or value of these entries as they are stored in prefs.
enum class SystemTheme {
  // Classic theme, used in the default or users' chosen theme.
  kDefault = 0,
#if BUILDFLAG(IS_LINUX)
  kGtk = 1,
  kQt = 2,
#endif
};

}  // namespace ui

#endif  // UI_COLOR_SYSTEM_THEME_H_
