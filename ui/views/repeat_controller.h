// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_REPEAT_CONTROLLER_H_
#define UI_VIEWS_REPEAT_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/views_export.h"

namespace base {
class TickClock;
}

namespace views {

///////////////////////////////////////////////////////////////////////////////
//
// RepeatController
//
//  An object that handles auto-repeating UI actions. There is a longer initial
//  delay after which point repeats become constant. Users provide a callback
//  that is notified when each repeat occurs so that they can perform the
//  associated action.
//
///////////////////////////////////////////////////////////////////////////////
class VIEWS_EXPORT RepeatController {
 public:
  explicit RepeatController(base::RepeatingClosure callback,
                            const base::TickClock* tick_clock = nullptr);

  RepeatController(const RepeatController&) = delete;
  RepeatController& operator=(const RepeatController&) = delete;

  virtual ~RepeatController();

  // Start repeating.
  void Start();

  // Stop repeating.
  void Stop();

  static constexpr base::TimeDelta GetInitialWaitForTesting() {
    return kInitialWait;
  }
  static constexpr base::TimeDelta GetRepeatingWaitForTesting() {
    return kRepeatingWait;
  }

  const base::OneShotTimer& timer_for_testing() const { return timer_; }

 private:
  // Initial time required before the first callback occurs.
  static constexpr base::TimeDelta kInitialWait = base::Milliseconds(250);

  // Period of callbacks after the first callback.
  static constexpr base::TimeDelta kRepeatingWait = base::Milliseconds(50);

  // Called when the timer expires.
  void Run();

  // The current timer.
  base::OneShotTimer timer_;

  base::RepeatingClosure callback_;
};

}  // namespace views

#endif  // UI_VIEWS_REPEAT_CONTROLLER_H_
