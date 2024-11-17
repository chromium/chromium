// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_L10N_UTIL_WIN_H_
#define UI_BASE_L10N_L10N_UTIL_WIN_H_

#include <windows.h>

#include <string>
#include <vector>

#include "base/component_export.h"

namespace gfx::win {
struct FontAdjustment;
}

namespace l10n_util {

// Give an HWND, this function sets the WS_EX_LAYOUTRTL extended style for the
// underlying window. When this style is set, the UI for the window is going to
// be mirrored. This is generally done for the UI of right-to-left languages
// such as Hebrew.
COMPONENT_EXPORT(UI_BASE) void HWNDSetRTLLayout(HWND hwnd);

// See https://devblogs.microsoft.com/oldnewthing/20050915-23/?p=34173
// and https://devblogs.microsoft.com/oldnewthing/20060626-11/?p=30743 as to why
// we need these next three functions.

COMPONENT_EXPORT(UI_BASE)
void AdjustUiFont(gfx::win::FontAdjustment& font_adjustment);

// Allow processes to override the configured locale with the user's Windows UI
// languages.  This function should generally be called once early in
// Application startup.
COMPONENT_EXPORT(UI_BASE) void OverrideLocaleWithUILanguageList();

// Retrieve the locale override, or an empty vector if the locale has not been
// or failed to be overridden.
COMPONENT_EXPORT(UI_BASE) const std::vector<std::string>& GetLocaleOverrides();

// Pulls resource string from the string bundle and returns it.
COMPONENT_EXPORT(UI_BASE) std::wstring GetWideString(int message_id);

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_WIN_H_
