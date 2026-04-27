// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/accelerator_observer.h"

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/events/event.h"
#include "ui/views/event_monitor.h"

namespace views::test {

AcceleratorObserver::AcceleratorObserver(ui::TrackedElement* target,
                                         gfx::NativeWindow window,
                                         ui::Accelerator accelerator)
    : target_(target),
      element_subscription_(
          ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
              target->identifier(),
              target->context(),
              base::BindRepeating(&AcceleratorObserver::OnElementDestroyed,
                                  base::Unretained(this)))),
      accelerator_(accelerator) {
  event_monitor_ = views::EventMonitor::CreateWindowMonitor(
      this, window, {ui::EventType::kKeyPressed});
}

AcceleratorObserver::~AcceleratorObserver() = default;

AcceleratorObserverState AcceleratorObserver::GetStateObserverInitialState()
    const {
  return AcceleratorObserverState::kWaiting;
}

void AcceleratorObserver::OnEvent(const ui::Event& event) {
  if (!event.IsKeyEvent()) {
    return;
  }

  const ui::KeyEvent& key_event = *event.AsKeyEvent();
  if (key_event.key_code() == accelerator_.key_code() &&
      key_event.flags() == accelerator_.modifiers()) {
    updated_ = true;
    OnStateObserverStateChanged(AcceleratorObserverState::kAcceleratorPressed);
    Cleanup();
  }
}

void AcceleratorObserver::OnElementDestroyed(ui::TrackedElement* element) {
  Cleanup();
}

void AcceleratorObserver::Cleanup() {
  target_ = nullptr;
  element_subscription_ = {};
  event_monitor_.reset();
  if (!updated_) {
    OnStateObserverStateChanged(AcceleratorObserverState::kTargetDestroyed);
    updated_ = true;
  }
}

}  // namespace views::test
