// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_enumerator.h"

#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"

namespace views {

WidgetEnumerator::WidgetEnumerator(Widget::Widgets widgets)
    : widgets_(std::move(widgets)) {
  for (Widget* widget : widgets_) {
    widget->AddObserver(this);
  }
}

WidgetEnumerator::~WidgetEnumerator() {
  // Remove any remaining observations.
  for (Widget* widget : widgets_) {
    widget->RemoveObserver(this);
  }
}

void WidgetEnumerator::OnWidgetDestroying(Widget* widget) {
  CHECK(base::Contains(widgets_, widget));
  widget->RemoveObserver(this);
  widgets_.erase(widget);
}

bool WidgetEnumerator::IsEmpty() const {
  return widgets_.empty();
}

Widget* WidgetEnumerator::Next() {
  Widget* widget = *widgets_.begin();
  widget->RemoveObserver(this);
  widgets_.erase(widgets_.begin());
  return widget;
}

}  // namespace views
