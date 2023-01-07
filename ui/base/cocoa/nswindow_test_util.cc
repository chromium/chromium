// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/nswindow_test_util.h"

namespace ui {

NSWindowFullscreenNotificationWaiter::NSWindowFullscreenNotificationWaiter(
    gfx::NativeWindow window)
    : window_(window) {
  // We only support one NSWindowFullscreenNotificationWaiter instance at once.
  DCHECK(!instance_);
  instance_ = this;
}

NSWindowFullscreenNotificationWaiter::~NSWindowFullscreenNotificationWaiter() {
  DCHECK(instance_ == this);
  instance_ = nullptr;
}

void NSWindowFullscreenNotificationWaiter::WaitForEnterAndExitCount(
    int target_enter_count,
    int target_exit_count) {
  target_enter_count_ = target_enter_count;
  target_exit_count_ = target_exit_count;

  // Do not wait if the targets are already met.
  if (enter_count_ >= target_enter_count_ &&
      exit_count_ >= target_exit_count_) {
    return;
  }

  run_loop_ = std::make_unique<base::RunLoop>();
  run_loop_->Run();
  run_loop_.reset();
}

void NSWindowFullscreenNotificationWaiter::NotifyFullscreenTransitionComplete(
    bool fullscreen) {
  if (fullscreen)
    enter_count_++;
  else
    exit_count_++;

  if (run_loop_ && enter_count_ >= target_enter_count_ &&
      exit_count_ >= target_exit_count_) {
    run_loop_->Quit();
  }
}

// static
void NSWindowFullscreenNotificationWaiter::NotifyFullscreenTransitionComplete(
    gfx::NativeWindow window,
    bool fullscreen) {
  if (!instance_ || instance_->window_ != window)
    return;
  instance_->NotifyFullscreenTransitionComplete(fullscreen);
}

// static
NSWindowFullscreenNotificationWaiter*
    NSWindowFullscreenNotificationWaiter::instance_ = nullptr;

// static
void NSWindowFakedForTesting::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

// static
bool NSWindowFakedForTesting::IsEnabled() {
  return enabled_;
}

// static
bool NSWindowFakedForTesting::enabled_ = false;

}  // namespace ui
