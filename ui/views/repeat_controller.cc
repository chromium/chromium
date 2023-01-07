// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/repeat_controller.h"

#include <utility>

namespace views {

///////////////////////////////////////////////////////////////////////////////
// RepeatController, public:

RepeatController::RepeatController(base::RepeatingClosure callback,
                                   const base::TickClock* tick_clock)
    : timer_(tick_clock), callback_(std::move(callback)) {}

RepeatController::~RepeatController() = default;

void RepeatController::Start() {
  // The first timer is slightly longer than subsequent repeats.
  timer_.Start(FROM_HERE, kInitialWait, this, &RepeatController::Run);
}

void RepeatController::Stop() {
  timer_.Stop();
}

///////////////////////////////////////////////////////////////////////////////
// RepeatController, private:

// static
constexpr base::TimeDelta RepeatController::kInitialWait;

// static
constexpr base::TimeDelta RepeatController::kRepeatingWait;

void RepeatController::Run() {
  timer_.Start(FROM_HERE, kRepeatingWait, this, &RepeatController::Run);
  callback_.Run();
}

}  // namespace views
