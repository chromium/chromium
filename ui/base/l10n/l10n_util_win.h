// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_L10N_L10N_UTIL_WIN_H_
#define UI_BASE_L10N_L10N_UTIL_WIN_H_

#include <windows.h>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "ui/base/ui_base_export.h"

namespace l10n_util {

// Returns the locale-dependent extended window styles.
// This function is used for adding locale-dependent extended window styles
// (e.g. WS_EX_LAYOUTRTL, WS_EX_RTLREADING, etc.) when creating a window.
// Callers should OR this value into their extended style value when creating
// a window.
UI_BASE_EXPORT int GetExtendedStyles();

// TODO(xji):
// This is a temporary name, it will eventually replace GetExtendedStyles
UI_BASE_EXPORT int GetExtendedTooltipStyles();

// Give an HWND, this function sets the WS_EX_LAYOUTRTL extended style for the
// underlying window. When this style is set, the UI for the window is going to
// be mirrored. This is generally done for the UI of right-to-left languages
// such as Hebrew.
UI_BASE_EXPORT void HWNDSetRTLLayout(HWND hwnd);

// See http://blogs.msdn.com/oldnewthing/archive/2005/09/15/467598.aspx
// and  http://blogs.msdn.com/oldnewthing/archive/2006/06/26/647365.aspx
// as to why we need these three functions.

// Return true if the default font (we get from Windows) is not suitable
// to use in the UI of the current UI (e.g. Malayalam, Bengali). If
// override_font_family and font_size_scaler are not null, they'll be
// filled with the font family name and the size scaler.  The output
// parameters are not modified if the return value is false.
UI_BASE_EXPORT bool NeedOverrideDefaultUIFont(
    base::string16* override_font_family,
    double* font_size_scaler);

// Allow processes to override the configured locale with the user's Windows UI
// languages.  This function should generally be called once early in
// Application startup.
UI_BASE_EXPORT void OverrideLocaleWithUILanguageList();

// Retrieve the locale override, or an empty vector if the locale has not been
// or failed to be overridden.
UI_BASE_EXPORT const std::vector<std::string>& GetLocaleOverrides();

}  // namespace l10n_util

#endif  // UI_BASE_L10N_L10N_UTIL_WIN_H_
