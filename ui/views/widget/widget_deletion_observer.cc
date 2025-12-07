// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_deletion_observer.h"

#include "base/scoped_observation.h"
#include "ui/views/widget/widget.h"

namespace views {

WidgetDeletionObserver::WidgetDeletionObserver(Widget* widget) {
  if (widget) {
    widget_observation_.Observe(widget);
  }
}

WidgetDeletionObserver::~WidgetDeletionObserver() {
  widget_observation_.Reset();
  CHECK(!IsInObserverList());
}

bool WidgetDeletionObserver::IsWidgetAlive() const {
  return widget_observation_.IsObserving();
}

void WidgetDeletionObserver::OnWidgetDestroying(Widget* widget) {
  widget_observation_.Reset();
}

}  // namespace views
