// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_ACTIVATION_DELEGATE_H_
#define UI_VIEWS_WIDGET_WIDGET_ACTIVATION_DELEGATE_H_

#include "ui/views/views_export.h"

namespace views {
class Widget;

// An interface to delegate activation control. This is intended to be
// implemented by a class that emulates widget activation in unittests.
class VIEWS_EXPORT WidgetActivationDelegate {
 public:
  static WidgetActivationDelegate* Get();
  WidgetActivationDelegate();
  WidgetActivationDelegate(const WidgetActivationDelegate&) = delete;
  WidgetActivationDelegate operator=(const WidgetActivationDelegate&) = delete;
  virtual ~WidgetActivationDelegate();

  // Adds the `widget` to the widget stack if it's not in the stack.  If
  // `activate` is true, this will activate the widget by moving it to the top
  // and activate if `activate`.  If the `widget` is already active, this is
  // no-op.
  virtual void MaybeActivate(Widget* widget, bool activate) = 0;

  // Deactivate given `widget`. This will search a next activatable widget from
  // the wiget stack and activate it if such widget exits. The activated widget
  // will be moved to the top of the widget stack. If the `widget` is not
  // currently active, this is no-op.
  virtual void Deactivate(Widget* widget) = 0;

  // Returns true if the `widget` is currently active.
  virtual bool IsActive(const Widget* widget) = 0;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_ACTIVATION_DELEGATE_H_
