// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_widget_observer.h"

#include "base/check_op.h"
#include "ui/views/widget/widget.h"

namespace views::test {

TestWidgetObserver::TestWidgetObserver(Widget* widget) : widget_(widget) {
  widget_observation_.Observe(widget_);
}

TestWidgetObserver::~TestWidgetObserver() = default;

void TestWidgetObserver::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(widget_, widget);
  widget_observation_.Reset();
  widget_ = nullptr;
}

}  // namespace views::test
