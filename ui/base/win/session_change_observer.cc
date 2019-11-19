// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/session_change_observer.h"

#include <wtsapi32.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/task/post_task.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace ui {

class SessionChangeObserver::WtsRegistrationNotificationManager {
 public:
  static WtsRegistrationNotificationManager* GetInstance() {
    return base::Singleton<WtsRegistrationNotificationManager>::get();
  }

  WtsRegistrationNotificationManager() {
    DCHECK(!singleton_hwnd_observer_);
    singleton_hwnd_observer_ = std::make_unique<gfx::SingletonHwndObserver>(
        base::BindRepeating(&WtsRegistrationNotificationManager::OnWndProc,
                            base::Unretained(this)));

    base::OnceClosure wts_register = base::BindOnce(
        base::IgnoreResult(&WTSRegisterSessionNotification),
        gfx::SingletonHwnd::GetInstance()->hwnd(), NOTIFY_FOR_THIS_SESSION);

    base::CreateCOMSTATaskRunner({base::ThreadPool()})
        ->PostTask(FROM_HERE, std::move(wts_register));
  }

  ~WtsRegistrationNotificationManager() { RemoveSingletonHwndObserver(); }

  void AddObserver(SessionChangeObserver* observer) {
    DCHECK(singleton_hwnd_observer_);
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(SessionChangeObserver* observer) {
    observer_list_.RemoveObserver(observer);
  }

 private:
  friend struct base::DefaultSingletonTraits<
      WtsRegistrationNotificationManager>;

  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
      case WM_WTSSESSION_CHANGE:
        for (SessionChangeObserver& observer : observer_list_)
          observer.OnSessionChange(wparam);
        break;
      case WM_DESTROY:
        RemoveSingletonHwndObserver();
        break;
    }
  }

  void RemoveSingletonHwndObserver() {
    if (!singleton_hwnd_observer_)
      return;

    singleton_hwnd_observer_.reset(nullptr);
    // There is no race condition between this code and the worker thread.
    // RemoveSingletonHwndObserver is only called from two places:
    //   1) Destruction due to Singleton Destruction.
    //   2) WM_DESTROY fired by SingletonHwnd.
    // Under both cases we are in shutdown, which means no other worker threads
    // can be running.
    WTSUnRegisterSessionNotification(gfx::SingletonHwnd::GetInstance()->hwnd());
    for (SessionChangeObserver& observer : observer_list_)
      observer.ClearCallback();
  }

  base::ObserverList<SessionChangeObserver, true>::Unchecked observer_list_;
  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;

  DISALLOW_COPY_AND_ASSIGN(WtsRegistrationNotificationManager);
};

SessionChangeObserver::SessionChangeObserver(const WtsCallback& callback)
    : callback_(callback) {
  DCHECK(!callback_.is_null());
  WtsRegistrationNotificationManager::GetInstance()->AddObserver(this);
}

SessionChangeObserver::~SessionChangeObserver() {
  ClearCallback();
}

void SessionChangeObserver::OnSessionChange(WPARAM wparam) {
  callback_.Run(wparam);
}

void SessionChangeObserver::ClearCallback() {
  if (!callback_.is_null()) {
    callback_.Reset();
    WtsRegistrationNotificationManager::GetInstance()->RemoveObserver(this);
  }
}

}  // namespace ui
