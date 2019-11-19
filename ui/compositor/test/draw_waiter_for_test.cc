// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/draw_waiter_for_test.h"

#include "ui/compositor/compositor.h"

namespace ui {

// static
void DrawWaiterForTest::WaitForCompositingStarted(Compositor* compositor) {
  DrawWaiterForTest waiter(WAIT_FOR_COMPOSITING_STARTED);
  waiter.WaitImpl(compositor);
}

void DrawWaiterForTest::WaitForCompositingEnded(Compositor* compositor) {
  DrawWaiterForTest waiter(WAIT_FOR_COMPOSITING_ENDED);
  waiter.WaitImpl(compositor);
}

// static
void DrawWaiterForTest::WaitForCommit(Compositor* compositor) {
  DrawWaiterForTest waiter(WAIT_FOR_COMMIT);
  waiter.WaitImpl(compositor);
}

DrawWaiterForTest::DrawWaiterForTest(WaitEvent wait_event)
    : wait_event_(wait_event) {
}

DrawWaiterForTest::~DrawWaiterForTest() {}

void DrawWaiterForTest::WaitImpl(Compositor* compositor) {
  compositor->AddObserver(this);
  wait_run_loop_ = std::make_unique<base::RunLoop>();
  wait_run_loop_->Run();
  compositor->RemoveObserver(this);
}

void DrawWaiterForTest::OnCompositingDidCommit(Compositor* compositor) {
  if (wait_event_ == WAIT_FOR_COMMIT)
    wait_run_loop_->Quit();
}

void DrawWaiterForTest::OnCompositingStarted(Compositor* compositor,
                                             base::TimeTicks start_time) {
  if (wait_event_ == WAIT_FOR_COMPOSITING_STARTED)
    wait_run_loop_->Quit();
}

void DrawWaiterForTest::OnCompositingEnded(Compositor* compositor) {
  if (wait_event_ == WAIT_FOR_COMPOSITING_ENDED)
    wait_run_loop_->Quit();
}

}  // namespace ui
