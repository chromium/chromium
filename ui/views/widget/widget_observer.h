// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_OBSERVER_H_
#define UI_VIEWS_WIDGET_WIDGET_OBSERVER_H_

#include "base/observer_list_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Rect;
}

namespace views {

class Widget;

// Observers can listen to various events on the Widgets.
class VIEWS_EXPORT WidgetObserver : public base::CheckedObserver {
 public:
  // The closing notification is sent immediately in response to (i.e. in the
  // same call stack as) a request to close the Widget (via Close() or
  // CloseNow()).
  virtual void OnWidgetClosing(Widget* widget) {}

  // Invoked after notification is received from the event loop that the native
  // widget has been created.
  virtual void OnWidgetCreated(Widget* widget) {}

  // The destroying event occurs immediately before the widget is destroyed.
  // This typically occurs asynchronously with respect the the close request, as
  // a result of a later invocation from the event loop.
  virtual void OnWidgetDestroying(Widget* widget) {}

  // Invoked after notification is received from the event loop that the native
  // widget has been destroyed.
  virtual void OnWidgetDestroyed(Widget* widget) {}

  // Called before RunShellDrag() is called and after it returns.
  virtual void OnWidgetDragWillStart(Widget* widget) {}
  virtual void OnWidgetDragComplete(Widget* widget) {}

  virtual void OnWidgetVisibilityChanging(Widget* widget, bool visible) {}
  virtual void OnWidgetVisibilityChanged(Widget* widget, bool visible) {}

  virtual void OnWidgetActivationChanged(Widget* widget, bool active) {}

  virtual void OnWidgetBoundsChanged(Widget* widget,
                                     const gfx::Rect& new_bounds) {}

 protected:
  ~WidgetObserver() override = default;
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_OBSERVER_H_
