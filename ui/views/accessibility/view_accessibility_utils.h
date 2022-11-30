// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_UTILS_H_
#define UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_UTILS_H_

#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

class VIEWS_EXPORT ViewAccessibilityUtils {
 public:
  // Returns a focused child widget if the view has a child that should be
  // treated as a special case. For example, if a tab modal dialog is visible
  // and focused, this will return the dialog when called on the BrowserView.
  // This helper function is used to treat such widgets as separate windows for
  // accessibility. Returns nullptr if no such widget is present.
  static Widget* GetFocusedChildWidgetForAccessibility(const View* view);

  // Used by GetFocusedChildWidgetForAccessibility to determine if a Widget
  // should be handled separately.
  static bool IsFocusedChildWidget(Widget* widget, const View* focused_view);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_VIEW_ACCESSIBILITY_UTILS_H_
