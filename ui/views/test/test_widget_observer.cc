// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_widget_observer.h"

#include "base/logging.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace test {

TestWidgetObserver::TestWidgetObserver(Widget* widget)
    : widget_(widget) {
  widget_->AddObserver(this);
}

TestWidgetObserver::~TestWidgetObserver() {
  if (widget_)
    widget_->RemoveObserver(this);
}

void TestWidgetObserver::OnWidgetDestroying(Widget* widget) {
  DCHECK_EQ(widget_, widget);
  widget_ = nullptr;
}

}  // namespace test
}  // namespace views
