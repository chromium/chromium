// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_SELECTION_EVENT_TYPE_H_
#define UI_TOUCH_SELECTION_SELECTION_EVENT_TYPE_H_

namespace ui {

// This file contains a list of events relating to selection and insertion, used
// for notifying Java when the renderer selection has changed.

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.touch_selection
enum SelectionEventType {
  SELECTION_HANDLES_SHOWN,
  SELECTION_HANDLES_MOVED,
  SELECTION_HANDLES_CLEARED,
  SELECTION_HANDLE_DRAG_STARTED,
  SELECTION_HANDLE_DRAG_STOPPED,
  INSERTION_HANDLE_SHOWN,
  INSERTION_HANDLE_MOVED,
  INSERTION_HANDLE_TAPPED,
  INSERTION_HANDLE_CLEARED,
  INSERTION_HANDLE_DRAG_STARTED,
  INSERTION_HANDLE_DRAG_STOPPED,
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_SELECTION_EVENT_TYPE_H_
