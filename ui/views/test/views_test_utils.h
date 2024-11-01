// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_UTILS_H_
#define UI_VIEWS_TEST_VIEWS_TEST_UTILS_H_

namespace views {

class View;
class Widget;

namespace test {

// Ensure that the entire Widget root view is properly laid out. This will
// call Widget::LayoutRootViewIfNecessary().
void RunScheduledLayout(Widget* widget);

// Ensure the given view is properly laid out. If the view is in a Widget view
// tree, invoke RunScheduledLayout(widget). Otherwise lay out the
// root parent view.
void RunScheduledLayout(View* view);

// Certain tests will fail when this experiment is running.
// TODO(crbug.com/329235190): Re-enable these tests and remove this function.
bool IsOzoneBubblesUsingPlatformWidgets();

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_UTILS_H_
