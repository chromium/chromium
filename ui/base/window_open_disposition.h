// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WINDOW_OPEN_DISPOSITION_H_
#define UI_BASE_WINDOW_OPEN_DISPOSITION_H_

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

#endif  // UI_BASE_WINDOW_OPEN_DISPOSITION_H_
