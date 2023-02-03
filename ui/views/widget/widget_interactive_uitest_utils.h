// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_INTERACTIVE_UITEST_UTILS_H_
#define UI_VIEWS_WIDGET_WIDGET_INTERACTIVE_UITEST_UTILS_H_

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace views::test {

// Wait until `callback` returns `expected_value`, but no longer than 1 second.
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
  PropertyWaiter(base::RepeatingCallback<bool(void)> callback,
                 bool expected_value);
  ~PropertyWaiter();

  bool Wait();

 private:
  void Check();

  const base::TimeDelta kTimeout = base::Seconds(1);
  base::RepeatingCallback<bool(void)> callback_;
  const bool expected_value_;
  bool success_ = false;
  base::TimeTicks start_time_;
  base::RunLoop run_loop_;
  base::RepeatingTimer timer_;
};

}  // namespace views::test

#endif  // UI_VIEWS_WIDGET_WIDGET_INTERACTIVE_UITEST_UTILS_H_
