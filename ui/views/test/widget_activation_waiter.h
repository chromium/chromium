// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WIDGET_ACTIVATION_WAITER_H_
#define UI_VIEWS_TEST_WIDGET_ACTIVATION_WAITER_H_

namespace views {

class Widget;

namespace test {

// Use in tests to wait until a Widget's activation changes to `active`.
void WaitForWidgetActive(Widget* widget, bool active);

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_TEST_WIDGET_ACTIVATION_WAITER_H_
