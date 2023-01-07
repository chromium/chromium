// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/window_event_dispatcher_test_api.h"

#include "base/run_loop.h"
#include "ui/aura/window_event_dispatcher.h"

namespace aura {
namespace test {

WindowEventDispatcherTestApi::WindowEventDispatcherTestApi(
    WindowEventDispatcher* dispatcher) : dispatcher_(dispatcher) {
}

bool WindowEventDispatcherTestApi::HoldingPointerMoves() const {
  return dispatcher_->move_hold_count_ > 0 || dispatcher_->held_move_event_;
}

void WindowEventDispatcherTestApi::WaitUntilPointerMovesDispatched() {
  if (!HoldingPointerMoves())
    return;
  base::RunLoop run_loop;
  dispatcher_->did_dispatch_held_move_event_callback_ = run_loop.QuitClosure();
  run_loop.Run();
  DCHECK(!HoldingPointerMoves());
}

}  // namespace test
}  // namespace aura

