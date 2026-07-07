// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_TEST_VIEWS_TEST_UTILS_H_
#define UI_VIEWS_TEST_VIEWS_TEST_UTILS_H_

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

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

// Wait until `callback` returns `expected_value`, but no longer than `timeout`
// seconds (defaults to 1s).
//
// Example Usage :
//  WidgetAutoclosePtr widget(CreateTopLevelNativeWidget());
//  PropertyWaiter minimize_waiter(
//      base::BindRepeating(
//          &Widget::IsMinimized, base::Unretained(widget.get())), true);
//  widget->Minimize();
//  EXPECT_TRUE(minimize_waiter.Wait());
class PropertyWaiter {
 public:
  PropertyWaiter(base::RepeatingCallback<bool()> callback,
                 bool expected_value,
                 base::TimeDelta timeout = base::Seconds(1));
  ~PropertyWaiter();

  bool Wait();

 private:
  void Check();

  base::TimeDelta timeout_;
  base::RepeatingCallback<bool(void)> callback_;
  const bool expected_value_;
  bool success_ = false;
  base::TimeTicks start_time_;
  base::RunLoop run_loop_;
  base::RepeatingTimer timer_;
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_TEST_VIEWS_TEST_UTILS_H_
