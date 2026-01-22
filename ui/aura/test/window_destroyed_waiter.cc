// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/window_destroyed_waiter.h"

#include <utility>

#include "base/check.h"
#include "base/run_loop.h"

namespace aura::test {

WindowDestroyedWaiter::WindowDestroyedWaiter(Window* window) {
  CHECK(window);
  observation_.Observe(window);
}

WindowDestroyedWaiter::~WindowDestroyedWaiter() = default;

void WindowDestroyedWaiter::Wait() {
  if (observation_.IsObserving()) {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

void WindowDestroyedWaiter::OnWindowDestroyed(Window* window) {
  CHECK(observation_.IsObserving());
  observation_.Reset();
  if (quit_closure_) {
    std::move(quit_closure_).Run();
  }
}

}  // namespace aura::test
