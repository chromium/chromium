// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_DISPLAY_CONFIG_HELPER_H_
#define UI_DISPLAY_WIN_DISPLAY_CONFIG_HELPER_H_

#include <windows.h>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/display/display_export.h"

namespace display::win {

DISPLAY_EXPORT absl::optional<DISPLAYCONFIG_PATH_INFO> GetDisplayConfigPathInfo(
    HMONITOR monitor);

}  // namespace display::win

#endif  // UI_DISPLAY_WIN_DISPLAY_CONFIG_HELPER_H_
