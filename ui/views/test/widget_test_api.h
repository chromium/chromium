// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WIDGET_TEST_API_H_
#define UI_VIEWS_TEST_WIDGET_TEST_API_H_

namespace views {
class Widget;

// Makes Widget::OnNativeWidgetActivationChanged return false, which prevents
// handling of the corresponding event (if the native widget implementation
// takes this into account).
void DisableActivationChangeHandlingForTests();

// WaitForAsyncWidgetRequests ensures that all visible side effects of all state
// changes (e.g. bounds change) for the given `widget` are observable from every
// part of chrome. For example, this means that this will wait for viz to
// produce all frames required as a result of updating the bounds of a widget.
void WaitForAsyncWidgetRequests(Widget& widget);

}  // namespace views

#endif  // UI_VIEWS_TEST_WIDGET_TEST_API_H_
