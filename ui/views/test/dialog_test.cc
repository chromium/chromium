// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/dialog_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views::test {

namespace {

DialogDelegate* DialogDelegateFor(Widget* widget) {
  auto* delegate = widget->widget_delegate()->AsDialogDelegate();
  return delegate;
}

}  // namespace

void AcceptDialog(Widget* widget) {
  WidgetDestroyedWaiter waiter(widget);
  DialogDelegateFor(widget)->AcceptDialog();
  waiter.Wait();
}

void CancelDialog(Widget* widget) {
  WidgetDestroyedWaiter waiter(widget);
  DialogDelegateFor(widget)->CancelDialog();
  waiter.Wait();
}

}  // namespace views::test
