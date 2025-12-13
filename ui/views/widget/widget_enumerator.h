// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_ENUMERATOR_H_
#define UI_VIEWS_WIDGET_WIDGET_ENUMERATOR_H_

#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {

// Use to iterate through Widgets and perform operations that might remove
// Widgets.
class WidgetEnumerator : public WidgetObserver {
 public:
  explicit WidgetEnumerator(Widget::Widgets widgets);
  WidgetEnumerator(const WidgetEnumerator&) = delete;
  WidgetEnumerator& operator=(const WidgetEnumerator&) = delete;
  ~WidgetEnumerator() override;

  bool IsEmpty() const;
  Widget* Next();

 private:
  void OnWidgetDestroying(Widget* widget) override;

  Widget::Widgets widgets_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_ENUMERATOR_H_
