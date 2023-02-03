// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/widget_interactive_uitest_utils.h"

#include <utility>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace views::test {

PropertyWaiter::PropertyWaiter(base::RepeatingCallback<bool(void)> callback,
                               bool expected_value)
    : callback_(std::move(callback)), expected_value_(expected_value) {}
PropertyWaiter::~PropertyWaiter() = default;

bool PropertyWaiter::Wait() {
  if (callback_.Run() == expected_value_) {
    success_ = true;
    return success_;
  }
  start_time_ = base::TimeTicks::Now();
  timer_.Start(FROM_HERE, base::TimeDelta(), this, &PropertyWaiter::Check);
  run_loop_.Run();
  return success_;
}

void PropertyWaiter::Check() {
  DCHECK(!success_);
  success_ = callback_.Run() == expected_value_;
  if (success_ || base::TimeTicks::Now() - start_time_ > kTimeout) {
    timer_.Stop();
    run_loop_.Quit();
  }
}

}  // namespace views::test
