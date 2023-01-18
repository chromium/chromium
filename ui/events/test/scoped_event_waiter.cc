// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/scoped_event_waiter.h"

#include "ui/events/event.h"
#include "ui/events/event_target.h"

namespace ui::test {

ScopedEventWaiter::ScopedEventWaiter(EventTarget* target,
                                     EventType target_event_type)
    : target_(target), target_event_type_(target_event_type) {
  DCHECK(target);
  target->AddPreTargetHandler(this);
}

ScopedEventWaiter::~ScopedEventWaiter() {
  target_->RemovePreTargetHandler(this);
}

void ScopedEventWaiter::Wait() {
  run_loop_.Run();
}

void ScopedEventWaiter::OnEvent(Event* event) {
  if (event->type() == target_event_type_) {
    run_loop_.Quit();
  }
}

}  // namespace ui::test
