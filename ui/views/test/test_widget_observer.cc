// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_widget_observer.h"

#include "base/check_op.h"
#include "ui/views/widget/widget.h"

namespace views::test {

TestWidgetObserver::TestWidgetObserver(Widget* widget) : widget_(widget) {
  widget_->AddObserver(this);
}

TestWidgetObserver::~TestWidgetObserver() {
  if (widget_)
    widget_->RemoveObserver(this);
  CHECK(!IsInObserverList());
}

void TestWidgetObserver::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(widget_, widget);
  widget_ = nullptr;
}

}  // namespace views::test
