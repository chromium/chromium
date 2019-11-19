// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_SHELL_H_
#define UI_BASE_WIN_SHELL_H_

#include <windows.h>

#include "base/strings/string16.h"
#include "ui/base/ui_base_export.h"

namespace base {
class FilePath;
}

namespace ui {
namespace win {

// Open the folder at |full_path| via the Windows shell. It is an error if
// |full_path| does not refer to a folder.
//
// Note: Must be called on a thread that allows blocking.
UI_BASE_EXPORT bool OpenFolderViaShell(const base::FilePath& full_path);

// Invokes the default verb on the file specified by |full_path| via the Windows
// shell. Usually, the default verb is "open" unless specified otherwise for the
// file type.
//
// In the event that there is no default application registered for the
// specified file, asks the user via the Windows "Open With" dialog.  Returns
// |true| on success.
//
// Note: Must be called on a thread that allows blocking.
UI_BASE_EXPORT bool OpenFileViaShell(const base::FilePath& full_path);

// Disables the ability of the specified window to be pinned to the taskbar or
// the Start menu. This will remove "Pin this program to taskbar" from the
// taskbar menu of the specified window.
UI_BASE_EXPORT bool PreventWindowFromPinning(HWND hwnd);

// Sets the application id, app icon, relaunch command and relaunch display name
// for the given window. |app_icon_index| should be set to 0 if the app icon
// file only has a single icon.
UI_BASE_EXPORT void SetAppDetailsForWindow(
    const base::string16& app_id,
    const base::FilePath& app_icon_path,
    int app_icon_index,
    const base::string16& relaunch_command,
    const base::string16& relaunch_display_name,
    HWND hwnd);

// Sets the application id given as the Application Model ID for the window
// specified.  This method is used to insure that different web applications
// do not group together on the Win7 task bar.
UI_BASE_EXPORT void SetAppIdForWindow(const base::string16& app_id, HWND hwnd);

// Sets the application icon for the window specified.
UI_BASE_EXPORT void SetAppIconForWindow(const base::FilePath& app_icon_path,
                                        int app_icon_index,
                                        HWND hwnd);

// Sets the relaunch command and relaunch display name for the window specified.
// Windows will use this information for grouping on the taskbar, and to create
// a shortcut if the window is pinned to the taskbar.
UI_BASE_EXPORT void SetRelaunchDetailsForWindow(
    const base::string16& relaunch_command,
    const base::string16& display_name,
    HWND hwnd);

// Clears the Window Property Store on an HWND.
UI_BASE_EXPORT void ClearWindowPropertyStore(HWND hwnd);

// Returns true if dwm composition is available and turned on on the current
// platform.
// This method supports a command-line override for testing.
UI_BASE_EXPORT bool IsAeroGlassEnabled();

// Returns true if dwm composition is available and turned on on the current
// platform.
UI_BASE_EXPORT bool IsDwmCompositionEnabled();

}  // namespace win
}  // namespace ui

#endif  // UI_BASE_WIN_SHELL_H_
