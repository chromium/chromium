// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/lock_screen/fake_lock_screen_controller.h"

#include <ostream>

namespace message_center {

FakeLockScreenController::FakeLockScreenController() = default;
FakeLockScreenController::~FakeLockScreenController() = default;

void FakeLockScreenController::DismissLockScreenThenExecute(
    base::OnceClosure pending_callback,
    base::OnceClosure cancel_callback,
    int message_id) {
  DCHECK(pending_callback) << "pending_callback must not be null";

  if (!is_screen_locked_) {
    DCHECK(!pending_callback_);
    DCHECK(!cancel_callback_);

    if (pending_callback)
      std::move(pending_callback).Run();
  } else {
    pending_callback_ = std::move(pending_callback);
    cancel_callback_ = std::move(cancel_callback);
  }
}

bool FakeLockScreenController::HasPendingCallback() {
  return !pending_callback_.is_null();
}

void FakeLockScreenController::SimulateUnlock() {
  DCHECK(is_screen_locked_);

  is_screen_locked_ = false;

  if (pending_callback_)
    std::move(pending_callback_).Run();
  std::move(cancel_callback_).Reset();
}

void FakeLockScreenController::CancelClick() {
  DCHECK(is_screen_locked_);

  pending_callback_.Reset();
  if (cancel_callback_)
    std::move(cancel_callback_).Run();
}

bool FakeLockScreenController::IsScreenLocked() const {
  return is_screen_locked_;
}

}  // namespace message_center
