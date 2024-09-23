// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_DISPLAY_CONFIG_HELPER_H_
#define UI_DISPLAY_WIN_DISPLAY_CONFIG_HELPER_H_

#include <windows.h>

#include <optional>

#include "ui/display/display_export.h"

namespace display::win {

DISPLAY_EXPORT std::optional<DISPLAYCONFIG_PATH_INFO> GetDisplayConfigPathInfo(
    HMONITOR monitor);

DISPLAY_EXPORT std::optional<DISPLAYCONFIG_PATH_INFO> GetDisplayConfigPathInfo(
    MONITORINFOEX monitor_info);

// Returns the manufacturer ID of the monitor identified by `path`. Returns 0 if
// `path` is nullopt or a failure occurred.
DISPLAY_EXPORT UINT16
GetDisplayManufacturerId(const std::optional<DISPLAYCONFIG_PATH_INFO>& path);

// Returns the manufacturer product code of the monitor identified by `path`.
// Returns 0 if `path` is nullopt or a failure occurred.
DISPLAY_EXPORT UINT16
GetDisplayProductCode(const std::optional<DISPLAYCONFIG_PATH_INFO>& path);

}  // namespace display::win

#endif  // UI_DISPLAY_WIN_DISPLAY_CONFIG_HELPER_H_
