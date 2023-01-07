// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_DELETION_OBSERVER_H_
#define UI_VIEWS_WIDGET_WIDGET_DELETION_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/views_export.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;

// A simple WidgetObserver that can be probed for the life of a widget.
class VIEWS_EXPORT WidgetDeletionObserver : public WidgetObserver {
 public:
  explicit WidgetDeletionObserver(Widget* widget);

  WidgetDeletionObserver(const WidgetDeletionObserver&) = delete;
  WidgetDeletionObserver& operator=(const WidgetDeletionObserver&) = delete;

  ~WidgetDeletionObserver() override;

  // Returns true if the widget passed in the constructor is still alive.
  bool IsWidgetAlive() { return widget_ != nullptr; }

  // Overridden from WidgetObserver.
  void OnWidgetDestroying(Widget* widget) override;

 private:
  void CleanupWidget();

  raw_ptr<Widget> widget_;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_DELETION_OBSERVER_H_
