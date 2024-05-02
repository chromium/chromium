// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_TEST_DRAW_WAITER_FOR_TEST_H_
#define UI_COMPOSITOR_TEST_DRAW_WAITER_FOR_TEST_H_

#include <memory>

#include "base/run_loop.h"
#include "ui/compositor/compositor_observer.h"

namespace ui {

class Compositor;

// This is only to be used for test. It allows execution of other tasks on
// the current message loop before the current task finishs (there is a
// potential for re-entrancy).
class DrawWaiterForTest : public CompositorObserver {
 public:
  DrawWaiterForTest(const DrawWaiterForTest&) = delete;
  DrawWaiterForTest& operator=(const DrawWaiterForTest&) = delete;

  // Waits for a draw to be issued by the compositor. If the test times out
  // here, there may be a logic error in the compositor code causing it
  // not to draw.
  static void WaitForCompositingStarted(Compositor* compositor);

  // Waits for a swap to be completed from the compositor.
  static void WaitForCompositingEnded(Compositor* compositor);

  // Waits for a commit instead of a draw.
  static void WaitForCommit(Compositor* compositor);

 private:
  enum WaitEvent {
    WAIT_FOR_COMMIT,
    WAIT_FOR_COMPOSITING_STARTED,
    WAIT_FOR_COMPOSITING_ENDED,
  };
  DrawWaiterForTest(WaitEvent wait_event);
  ~DrawWaiterForTest() override;

  void WaitImpl(Compositor* compositor);

  // CompositorObserver implementation.
  void OnCompositingDidCommit(Compositor* compositor) override;
  void OnCompositingStarted(Compositor* compositor,
                            base::TimeTicks start_time) override;
  void OnCompositingAckDeprecated(Compositor* compositor) override;

  std::unique_ptr<base::RunLoop> wait_run_loop_;

  WaitEvent wait_event_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_TEST_DRAW_WAITER_FOR_TEST_H_
