// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COLOR_COLOR_SWITCHES_H_
#define UI_COLOR_COLOR_SWITCHES_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace switches {

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(COLOR_SWITCHES)
extern const char kPervasiveSystemAccentColor[];
#endif

}  // namespace switches

#endif  // UI_COLOR_COLOR_SWITCHES_H_
