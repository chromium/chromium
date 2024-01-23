// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/begin_main_frame_waiter.h"

#include "ui/compositor/compositor.h"

namespace ui {

BeginMainFrameWaiter::BeginMainFrameWaiter(ui::Compositor* compositor)
    : compositor_(compositor),
      run_loop_(base::RunLoop::Type::kNestableTasksAllowed) {
  compositor_->AddObserver(this);
}

BeginMainFrameWaiter::~BeginMainFrameWaiter() {
  compositor_->RemoveObserver(this);
}

void BeginMainFrameWaiter::OnDidBeginMainFrame(ui::Compositor* compositor) {
  DCHECK_EQ(compositor_, compositor);
  begin_main_frame_received_ = true;
  if (run_loop_.running()) {
    run_loop_.Quit();
  }
}

void BeginMainFrameWaiter::Wait() {
  if (begin_main_frame_received_) {
    return;
  }
  if (!run_loop_.running()) {
    run_loop_.Run();
  }
}

}  // namespace ui
