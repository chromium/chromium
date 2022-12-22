// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_WM_CORE_SWITCHES_H_
#define UI_WM_CORE_WM_CORE_SWITCHES_H_

#include "base/component_export.h"
#include "build/build_config.h"

namespace wm {
namespace switches {

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see `ash::GetOffTheRecordCommandLine()`.)

// Please keep alphabetized.
COMPONENT_EXPORT(UI_WM) extern const char kWindowAnimationsDisabled[];

}  // namespace switches
}  // namespace wm

#endif  // UI_WM_CORE_WM_CORE_SWITCHES_H_
