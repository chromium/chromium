// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/platform_event_waiter.h"

#include <utility>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/events/platform/platform_event_source.h"

namespace ui {

PlatformEventWaiter::PlatformEventWaiter(base::OnceClosure success_callback,
                                         PlatformEventMatcher event_matcher)
    : success_callback_(std::move(success_callback)),
      event_matcher_(std::move(event_matcher)) {
  PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
}

PlatformEventWaiter::~PlatformEventWaiter() {
  PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
}

void PlatformEventWaiter::WillProcessEvent(const PlatformEvent& event) {
  if (event_matcher_.Run(event)) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(success_callback_));
    delete this;
  }
}

void PlatformEventWaiter::DidProcessEvent(const PlatformEvent& event) {
}

// static
PlatformEventWaiter* PlatformEventWaiter::Create(
    base::OnceClosure success_callback,
    PlatformEventMatcher event_matcher) {
  return new PlatformEventWaiter(std::move(success_callback),
                                 std::move(event_matcher));
}

}  // namespace ui
