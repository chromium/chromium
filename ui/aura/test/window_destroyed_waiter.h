// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_WINDOW_DESTROYED_WAITER_H_
#define UI_AURA_TEST_WINDOW_DESTROYED_WAITER_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace aura::test {

// Utility to wait for the given window to be destroyed.
class WindowDestroyedWaiter : public WindowObserver {
 public:
  explicit WindowDestroyedWaiter(Window* window);
  WindowDestroyedWaiter(const WindowDestroyedWaiter&) = delete;
  WindowDestroyedWaiter& operator=(const WindowDestroyedWaiter&) = delete;
  ~WindowDestroyedWaiter() override;

  // Wait until the window passed to the ctor is destroyed.
  // If the window is already destroyed after the construction of this instance,
  // this does not block.
  void Wait();

  // WindowObserver:
  void OnWindowDestroyed(Window* window) override;

 private:
  base::ScopedObservation<Window, WindowObserver> observation_{this};
  base::OnceClosure quit_closure_;
};

}  // namespace aura::test

#endif  // UI_AURA_TEST_WINDOW_DESTROYED_WAITER_H_
