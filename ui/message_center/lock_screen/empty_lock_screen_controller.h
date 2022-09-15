// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_LOCK_SCREEN_EMPTY_LOCK_SCREEN_CONTROLLER_H_
#define UI_MESSAGE_CENTER_LOCK_SCREEN_EMPTY_LOCK_SCREEN_CONTROLLER_H_

#include "ui/message_center/lock_screen/lock_screen_controller.h"

namespace message_center {

class EmptyLockScreenController : public LockScreenController {
 public:
  EmptyLockScreenController() = default;

  EmptyLockScreenController(const EmptyLockScreenController&) = delete;
  EmptyLockScreenController& operator=(const EmptyLockScreenController&) =
      delete;

  ~EmptyLockScreenController() override = default;

  void DismissLockScreenThenExecute(base::OnceClosure pending_callback,
                                    base::OnceClosure cancal_callback,
                                    int message_id) override;
  bool IsScreenLocked() const override;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_LOCK_SCREEN_EMPTY_LOCK_SCREEN_CONTROLLER_H_
