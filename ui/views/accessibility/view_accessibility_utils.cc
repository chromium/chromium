// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_accessibility_utils.h"

#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {

// static
Widget* ViewAccessibilityUtils::GetFocusedChildWidgetForAccessibility(
    const View* view) {
  const FocusManager* focus_manager = view->GetFocusManager();
  if (!focus_manager)
    return nullptr;
  const View* focused_view = view->GetFocusManager()->GetFocusedView();
  if (!focused_view)
    return nullptr;

  std::set<Widget*> child_widgets;
  Widget::GetAllOwnedWidgets(view->GetWidget()->GetNativeView(),
                             &child_widgets);
  const auto i =
      std::find_if(child_widgets.cbegin(), child_widgets.cend(),
                   [focused_view](auto* child_widget) {
                     return IsFocusedChildWidget(child_widget, focused_view);
                   });
  return (i == child_widgets.cend()) ? nullptr : *i;
}

// static
bool ViewAccessibilityUtils::IsFocusedChildWidget(Widget* widget,
                                                  const View* focused_view) {
  return widget->IsVisible() &&
         widget->GetContentsView()->Contains(focused_view);
}

}  // namespace views
