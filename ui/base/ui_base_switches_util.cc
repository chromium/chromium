// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches_util.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/ui_base_switches.h"

namespace switches {

bool IsTouchDragDropEnabled() {
  const auto* const command_line = base::CommandLine::ForCurrentProcess();
#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_ANDROID)
  return !command_line->HasSwitch(kDisableTouchDragDrop);
#else
  return command_line->HasSwitch(kEnableTouchDragDrop);
#endif
}

}  // namespace switches
