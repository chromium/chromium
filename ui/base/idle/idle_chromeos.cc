// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "ui/base/idle/idle_internal.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ui {

namespace {

// A class that bridges PowerManagerClient's idle signals with Idle's observer.
class PowerManagerClientObserverImpl
    : public ash::SessionManagerClient::Observer {
 public:
  static PowerManagerClientObserverImpl* GetInstance() {
    static base::NoDestructor<PowerManagerClientObserverImpl> instance;
    return instance.get();
  }

  PowerManagerClientObserverImpl(const PowerManagerClientObserverImpl&) =
      delete;
  PowerManagerClientObserverImpl& operator=(
      const PowerManagerClientObserverImpl&) = delete;

  base::CallbackListSubscription AddCallback(
      base::RepeatingCallback<void(bool)> callback) {
    return callbacks_.Add(std::move(callback));
  }

 private:
  friend class base::NoDestructor<PowerManagerClientObserverImpl>;

  PowerManagerClientObserverImpl() {
    if (ash::SessionManagerClient::Get()) {
      ash::SessionManagerClient::Get()->AddObserver(this);
    }
  }

  ~PowerManagerClientObserverImpl() override {
    if (ash::SessionManagerClient::Get()) {
      ash::SessionManagerClient::Get()->RemoveObserver(this);
    }
  }

  void ScreenLockedStateUpdated() override {
    bool is_locked = ash::SessionManagerClient::Get()->IsScreenLocked();
    callbacks_.Notify(is_locked);
  }

  base::RepeatingCallbackList<void(bool)> callbacks_;
};

}  // namespace

base::CallbackListSubscription AddScreenLockCallback(
    base::RepeatingCallback<void(bool)> callback) {
  return PowerManagerClientObserverImpl::GetInstance()->AddCallback(
      std::move(callback));
}

int CalculateIdleTime() {
  const base::TimeDelta idle_time =
      base::TimeTicks::Now() -
      ui::UserActivityDetector::Get()->last_activity_time();
  return static_cast<int>(idle_time.InSeconds());
}

bool CheckIdleStateIsLocked() {
  if (IdleStateForTesting().has_value())
    return IdleStateForTesting().value() == IDLE_STATE_LOCKED;

  // Note that the client can be null in e.g. Ash unit tests, which can cause a
  // crash if a browser or OS subsystem wants to know if the computer is locked.
  //
  // If it is not possible to check the locked state, assume the system isn't
  // locked.
  return ash::SessionManagerClient::Get() &&
         ash::SessionManagerClient::Get()->IsScreenLocked();
}

}  // namespace ui
