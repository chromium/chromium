// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_WIDGET_SHOW_STATE_WAITER_H_
#define UI_VIEWS_TEST_WIDGET_SHOW_STATE_WAITER_H_

#include "ui/base/mojom/window_show_state.mojom.h"

namespace views {

class Widget;

namespace test {

void WaitForWidgetShowState(Widget* widget,
                            ui::mojom::WindowShowState show_state);

}  // namespace test

}  // namespace views

#endif  // UI_VIEWS_TEST_WIDGET_SHOW_STATE_WAITER_H_
