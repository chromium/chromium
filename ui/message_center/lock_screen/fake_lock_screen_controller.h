// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_LOCK_SCREEN_FAKE_LOCK_SCREEN_CONTROLLER_H_
#define UI_MESSAGE_CENTER_LOCK_SCREEN_FAKE_LOCK_SCREEN_CONTROLLER_H_

#include "ui/message_center/lock_screen/lock_screen_controller.h"

namespace message_center {

class FakeLockScreenController : public LockScreenController {
 public:
  FakeLockScreenController();

  FakeLockScreenController(const FakeLockScreenController&) = delete;
  FakeLockScreenController& operator=(const FakeLockScreenController&) = delete;

  ~FakeLockScreenController() override;

  void DismissLockScreenThenExecute(base::OnceClosure pending_callback,
                                    base::OnceClosure cancal_callback,
                                    int message_id) override;
  bool IsScreenLocked() const override;

  // Methods for tests:
  void set_is_screen_locked(bool locked) { is_screen_locked_ = locked; }
  bool HasPendingCallback();
  void SimulateUnlock();
  void CancelClick();

 private:
  bool is_screen_locked_ = false;
  base::OnceClosure pending_callback_;
  base::OnceClosure cancel_callback_;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_LOCK_SCREEN_FAKE_LOCK_SCREEN_CONTROLLER_H_
