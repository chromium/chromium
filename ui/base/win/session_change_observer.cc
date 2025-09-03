// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/session_change_observer.h"

#include <wtsapi32.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/task/thread_pool.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace ui {

class SessionChangeObserver::WtsRegistrationNotificationManager {
 public:
  static WtsRegistrationNotificationManager* GetInstance() {
    return base::Singleton<WtsRegistrationNotificationManager>::get();
  }

  WtsRegistrationNotificationManager() {
    DCHECK(!hwnd_subscription_);
    hwnd_subscription_ = gfx::SingletonHwnd::GetInstance()->RegisterCallback(
        base::BindRepeating(&WtsRegistrationNotificationManager::OnWndProc,
                            base::Unretained(this)));

    base::OnceClosure wts_register =
        base::BindOnce(base::IgnoreResult(&WTSRegisterSessionNotification),
                       gfx::SingletonHwnd::GetInstance()->hwnd(),
                       DWORD{NOTIFY_FOR_THIS_SESSION});

    base::ThreadPool::CreateCOMSTATaskRunner({})->PostTask(
        FROM_HERE, std::move(wts_register));
  }

  WtsRegistrationNotificationManager(
      const WtsRegistrationNotificationManager&) = delete;
  WtsRegistrationNotificationManager& operator=(
      const WtsRegistrationNotificationManager&) = delete;

  ~WtsRegistrationNotificationManager() { ResetHwndSubscription(); }

  void AddObserver(SessionChangeObserver* observer) {
    DCHECK(hwnd_subscription_);
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
        if (wparam == WTS_SESSION_LOCK || wparam == WTS_SESSION_UNLOCK) {
          bool is_current_session;
          const bool* is_current_session_ptr = &is_current_session;
          DWORD current_session_id = 0;
          if (!::ProcessIdToSessionId(::GetCurrentProcessId(),
                                      &current_session_id)) {
            PLOG(ERROR) << "ProcessIdToSessionId failed";
            is_current_session_ptr = nullptr;
          } else {
            is_current_session =
                (static_cast<DWORD>(lparam) == current_session_id);
          }
          observer_list_.Notify(&SessionChangeObserver::OnSessionChange, wparam,
                                is_current_session_ptr);
        }
        break;
      case WM_DESTROY:
        ResetHwndSubscription();
        break;
    }
  }

  void ResetHwndSubscription() {
    if (!hwnd_subscription_) {
      return;
    }

    hwnd_subscription_.reset();
    // There is no race condition between this code and the worker thread.
    // ResetHwndSubscription is only called from two places:
    //   1) Destruction due to Singleton Destruction.
    //   2) WM_DESTROY fired by SingletonHwnd.
    // Under both cases we are in shutdown, which means no other worker threads
    // can be running.
    WTSUnRegisterSessionNotification(gfx::SingletonHwnd::GetInstance()->hwnd());
    observer_list_.Notify(&SessionChangeObserver::ClearCallback);
  }

  base::ObserverList<SessionChangeObserver, true>::Unchecked observer_list_;
  std::optional<base::CallbackListSubscription> hwnd_subscription_;
};

SessionChangeObserver::SessionChangeObserver(const WtsCallback& callback)
    : callback_(callback) {
  DCHECK(!callback_.is_null());
  WtsRegistrationNotificationManager::GetInstance()->AddObserver(this);
}

SessionChangeObserver::~SessionChangeObserver() {
  ClearCallback();
}

void SessionChangeObserver::OnSessionChange(WPARAM wparam,
                                            const bool* is_current_session) {
  callback_.Run(wparam, is_current_session);
}

void SessionChangeObserver::ClearCallback() {
  if (!callback_.is_null()) {
    callback_.Reset();
    WtsRegistrationNotificationManager::GetInstance()->RemoveObserver(this);
  }
}

}  // namespace ui
