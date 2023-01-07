// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_TOUCH_SELECTION_TOUCH_HANDLE_ORIENTATION_H_
#define UI_TOUCH_SELECTION_TOUCH_HANDLE_ORIENTATION_H_

namespace ui {

// Orientation types for Touch handles, used for setting the type of
// handle orientation on java and native side.

// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.ui.touch_selection
enum class TouchHandleOrientation {
  LEFT,
  CENTER,
  RIGHT,
  UNDEFINED,
};

}  // namespace ui

#endif  // UI_TOUCH_SELECTION_TOUCH_HANDLE_ORIENTATION_H_
