// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/repeat_controller.h"

#include <utility>

using base::TimeDelta;

namespace views {

///////////////////////////////////////////////////////////////////////////////
// RepeatController, public:

RepeatController::RepeatController(base::RepeatingClosure callback)
    : callback_(std::move(callback)) {}

RepeatController::~RepeatController() = default;

void RepeatController::Start() {
  // The first timer is slightly longer than subsequent repeats.
  timer_.Start(FROM_HERE, TimeDelta::FromMilliseconds(250), this,
               &RepeatController::Run);
}

void RepeatController::Stop() {
  timer_.Stop();
}

///////////////////////////////////////////////////////////////////////////////
// RepeatController, private:

void RepeatController::Run() {
  timer_.Start(FROM_HERE, TimeDelta::FromMilliseconds(50), this,
               &RepeatController::Run);
  callback_.Run();
}

}  // namespace views
