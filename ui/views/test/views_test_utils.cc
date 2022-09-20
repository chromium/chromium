// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/views_test_utils.h"

#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views::test {

void RunScheduledLayout(Widget* widget) {
  DCHECK(widget);
  widget->LayoutRootViewIfNecessary();
}

void RunScheduledLayout(View* view) {
  DCHECK(view);
  Widget* widget = view->GetWidget();
  if (widget) {
    RunScheduledLayout(widget);
    return;
  }
  View* parent_view = view;
  while (parent_view->parent())
    parent_view = parent_view->parent();
  if (parent_view->needs_layout())
    parent_view->Layout();
}

}  // namespace views::test
