// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WINDOW_OPEN_DISPOSITION_H_
#define UI_BASE_WINDOW_OPEN_DISPOSITION_H_

#include "base/component_export.h"

enum class WindowOpenDisposition {
  UNKNOWN,
  CURRENT_TAB,
  // Indicates that only one tab with the url should exist in the same window.
  SINGLETON_TAB,
  NEW_FOREGROUND_TAB,
  NEW_BACKGROUND_TAB,
  NEW_POPUP,
  NEW_WINDOW,
  SAVE_TO_DISK,
  OFF_THE_RECORD,
  IGNORE_ACTION,
  // Activates an existing tab containing the url, rather than navigating.
  // This is similar to SINGLETON_TAB, but searches across all windows from
  // the current profile and anonymity (instead of just the current one);
  // closes the current tab on switching if the current tab was the NTP with
  // no session history; and behaves like CURRENT_TAB instead of
  // NEW_FOREGROUND_TAB when no existing tab is found.
  SWITCH_TO_TAB,
  // Creates a new document picture-in-picture window showing a child WebView.
  NEW_PICTURE_IN_PICTURE,
  // Update when adding a new disposition.
  MAX_VALUE = NEW_PICTURE_IN_PICTURE,
};

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

#endif  // UI_BASE_WINDOW_OPEN_DISPOSITION_H_
