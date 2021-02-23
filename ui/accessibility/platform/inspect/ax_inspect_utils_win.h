// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_WIN_H_
#define UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_WIN_H_

#include <stdint.h>
#include <wtypes.h>

#include <string>
#include <vector>

#include "base/process/process_handle.h"
#include "ui/accessibility/ax_export.h"
#include "ui/gfx/win/hwnd_util.h"

namespace ui {
struct AXTreeSelector;

AX_EXPORT std::wstring IAccessibleRoleToString(int32_t ia_role);
AX_EXPORT std::wstring IAccessible2RoleToString(int32_t ia_role);
AX_EXPORT std::wstring IAccessibleStateToString(int32_t ia_state);
AX_EXPORT void IAccessibleStateToStringVector(
    int32_t ia_state,
    std::vector<std::wstring>* result);
AX_EXPORT std::wstring IAccessible2StateToString(int32_t ia2_state);
AX_EXPORT void IAccessible2StateToStringVector(
    int32_t ia_state,
    std::vector<std::wstring>* result);

// Handles both IAccessible/MSAA events and IAccessible2 events.
AX_EXPORT std::wstring AccessibilityEventToString(int32_t event_id);

AX_EXPORT std::wstring UiaIdentifierToString(int32_t identifier);
AX_EXPORT std::wstring UiaOrientationToString(int32_t identifier);
AX_EXPORT std::wstring UiaLiveSettingToString(int32_t identifier);

AX_EXPORT std::string BstrToUTF8(BSTR bstr);
AX_EXPORT std::string UiaIdentifierToStringUTF8(int32_t id);

AX_EXPORT HWND GetHwndForProcess(base::ProcessId pid);

// Returns HWND of window matching a given tree selector.
AX_EXPORT HWND GetHWNDBySelector(const ui::AXTreeSelector& selector);

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_INSPECT_AX_INSPECT_UTILS_WIN_H_
