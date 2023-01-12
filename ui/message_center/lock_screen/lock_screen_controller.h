// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_LOCK_SCREEN_LOCK_SCREEN_CONTROLLER_H_
#define UI_MESSAGE_CENTER_LOCK_SCREEN_LOCK_SCREEN_CONTROLLER_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "ui/message_center/message_center_export.h"

namespace message_center {

// This class is CONTROLLERd the lock-screen related behavior from
// MessageCenterImpl.
class MESSAGE_CENTER_EXPORT LockScreenController {
 public:
  virtual ~LockScreenController() = default;

  // Processes a closure which runs after the device is unlocked. If the device
  // is locked, the pending closure is run just after the device is unlocked.
  // If it's not locked at the time, the pending callback is run immidiately.
  // If the pending callback is cancelled without being run for some reason,
  // the cancel callback is run. In other words, onlt one of the pending or
  // cancel callback must be run. The cancel callback may be run even when the
  // device is locked.
  virtual void DismissLockScreenThenExecute(base::OnceClosure pending_callback,
                                            base::OnceClosure cancel_callback,
                                            int message_id = -1) = 0;

  // Returns the status of the device lock. True if locked, false otherwise.
  virtual bool IsScreenLocked() const = 0;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_LOCK_SCREEN_LOCK_SCREEN_CONTROLLER_H_
