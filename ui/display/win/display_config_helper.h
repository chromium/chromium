// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_DISPLAY_CONFIG_HELPER_H_
#define UI_DISPLAY_WIN_DISPLAY_CONFIG_HELPER_H_

#include <windows.h>

#include <optional>
#include <string_view>

#include "ui/display/display_export.h"

namespace display::win {

// Return a string view from a fixed-length array representing a string, up
// until the first nul terminator, if any.
template <size_t N>
std::wstring_view FixedArrayToStringView(
    const std::wstring_view::value_type (&str)[N]) {
  return std::wstring_view(str, ::wcsnlen_s(str, N));
}

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
