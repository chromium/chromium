// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_deletion_observer.h"

#include "ui/views/widget/widget.h"

namespace views {

WidgetDeletionObserver::WidgetDeletionObserver(Widget* widget)
    : widget_(widget) {
  if (widget_)
    widget_->AddObserver(this);
}

WidgetDeletionObserver::~WidgetDeletionObserver() {
  CleanupWidget();
}

void WidgetDeletionObserver::OnWidgetDestroying(Widget* widget) {
  CleanupWidget();
}

void WidgetDeletionObserver::CleanupWidget() {
  if (widget_) {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
  }
}

}  // namespace views
