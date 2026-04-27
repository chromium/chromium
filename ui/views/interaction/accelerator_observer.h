// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INTERACTION_ACCELERATOR_OBSERVER_H_
#define UI_VIEWS_INTERACTION_ACCELERATOR_OBSERVER_H_

#include <memory>

#include "base/callback_list.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/state_observer.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/native_ui_types.h"

namespace ui {
class Event;
class TrackedElement;
}  // namespace ui

namespace views {
class EventMonitor;
}

namespace views::test {

enum class AcceleratorObserverState {
  kWaiting,
  kTargetDestroyed,
  kAcceleratorPressed,
};

// Observes the surface containing `el` for an expected `accelerator`, or for
// the element to be destroyed.
class AcceleratorObserver
    : public ui::test::StateObserver<AcceleratorObserverState>,
      public ui::EventObserver {
 public:
  AcceleratorObserver(ui::TrackedElement* target,
                      gfx::NativeWindow window,
                      ui::Accelerator accelerator);
  ~AcceleratorObserver() override;

 private:
  // ui::test::StateObserver:
  AcceleratorObserverState GetStateObserverInitialState() const override;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

  void OnElementDestroyed(ui::TrackedElement* element);

  void Cleanup();

  raw_ptr<ui::TrackedElement> target_ = nullptr;
  base::CallbackListSubscription element_subscription_;
  std::unique_ptr<views::EventMonitor> event_monitor_;
  bool updated_ = false;
  const ui::Accelerator accelerator_;
};

}  // namespace views::test

#endif  // UI_VIEWS_INTERACTION_ACCELERATOR_OBSERVER_H_
