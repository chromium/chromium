// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WIDGET_TEST_API_H_
#define UI_VIEWS_TEST_WIDGET_TEST_API_H_

#include "base/memory/raw_ref.h"

namespace views {
class Widget;

// Makes Widget::OnNativeWidgetActivationChanged return false, which prevents
// handling of the corresponding event (if the native widget implementation
// takes this into account).
void DisableActivationChangeHandlingForTests();

// AsyncWidgetRequestWaiter ensures that all visible side effects of all state
// changes (e.g. bounds change) as a result of all actions taken after its
// construction and before `Wait` is called on it are observable from every part
// of chrome. For example, this means that this will wait for viz to produce all
// frames required as a result of updating the bounds of a widget.
class AsyncWidgetRequestWaiter {
 public:
  explicit AsyncWidgetRequestWaiter(Widget& widget);
  ~AsyncWidgetRequestWaiter();

  AsyncWidgetRequestWaiter(const AsyncWidgetRequestWaiter&) = delete;
  AsyncWidgetRequestWaiter& operator=(const AsyncWidgetRequestWaiter&) = delete;
  AsyncWidgetRequestWaiter(AsyncWidgetRequestWaiter&&) = delete;
  AsyncWidgetRequestWaiter& operator=(AsyncWidgetRequestWaiter&&) = delete;

  // Waits for all side effects of all state changes made since the construction
  // of this `AsyncWidgetRequestWaiter`.
  void Wait();

 private:
  const raw_ref<Widget> widget_;
  bool waited_ = false;
};

}  // namespace views

#endif  // UI_VIEWS_TEST_WIDGET_TEST_API_H_
