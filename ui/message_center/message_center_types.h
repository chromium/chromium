// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_MESSAGE_CENTER_TYPES_H_
#define UI_MESSAGE_CENTER_MESSAGE_CENTER_TYPES_H_

namespace message_center {

enum Visibility {
  // When nothing or just toast popups are being displayed.
  VISIBILITY_TRANSIENT = 0,
  // When the message center view is being displayed.
  VISIBILITY_MESSAGE_CENTER,
};

// Identifies the source of a displayed notification.
enum DisplaySource {
  // Displayed from the message center.
  DISPLAY_SOURCE_MESSAGE_CENTER = 0,
  // Displayed as a popup.
  DISPLAY_SOURCE_POPUP,
};

// Enumeration of quiet mode set by user or focus mode.
enum class QuietModeSourceType {
  kUserAction,
  kFocusMode,
  kMaxValue = kFocusMode,
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_MESSAGE_CENTER_TYPES_H_
