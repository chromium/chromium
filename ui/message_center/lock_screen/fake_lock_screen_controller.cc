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
    CHECK(!pending_callback_, base::NotFatalUntil::M163);
    CHECK(!cancel_callback_, base::NotFatalUntil::M163);

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
  CHECK(is_screen_locked_, base::NotFatalUntil::M163);

  is_screen_locked_ = false;

  if (pending_callback_)
    std::move(pending_callback_).Run();
  std::move(cancel_callback_).Reset();
}

void FakeLockScreenController::CancelClick() {
  CHECK(is_screen_locked_, base::NotFatalUntil::M163);

  pending_callback_.Reset();
  if (cancel_callback_)
    std::move(cancel_callback_).Run();
}

bool FakeLockScreenController::IsScreenLocked() const {
  return is_screen_locked_;
}

bool FakeLockScreenController::IsNotificationAllowedOnLockScreen(
    const std::string& id) const {
  return is_notification_allowed_on_lock_screen_;
}

}  // namespace message_center
