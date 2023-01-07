// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WINDOW_OPEN_DISPOSITION_UTILS_H_
#define UI_BASE_WINDOW_OPEN_DISPOSITION_UTILS_H_

#include "base/component_export.h"
#include "ui/base/window_open_disposition.h"

namespace ui {

// Translates event flags from a click on a link into the user's desired window
// disposition.  For example, a middle click would mean to open a background
// tab.  |disposition_for_current_tab| is the disposition to return if the flags
// suggest opening in the current tab; for example, a caller could set this to
// NEW_FOREGROUND_TAB to prevent a click from overwriting the current tab by
// default.
COMPONENT_EXPORT(UI_BASE)
WindowOpenDisposition DispositionFromClick(
    bool middle_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key,
    WindowOpenDisposition disposition_for_current_tab =
        WindowOpenDisposition::CURRENT_TAB);

// As with DispositionFromClick(), but using |event_flags| as in ui::MouseEvent.
COMPONENT_EXPORT(UI_BASE)
WindowOpenDisposition DispositionFromEventFlags(
    int event_flags,
    WindowOpenDisposition disposition_for_current_tab =
        WindowOpenDisposition::CURRENT_TAB);

}  // namespace ui

#endif  // UI_BASE_WINDOW_OPEN_DISPOSITION_UTILS_H_
