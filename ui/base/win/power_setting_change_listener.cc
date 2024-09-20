// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/power_setting_change_listener.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "ui/gfx/win/singleton_hwnd.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace ui {

class PowerSettingChangeObserver {
 public:
  static PowerSettingChangeObserver* GetInstance();

  void AddListener(PowerSettingChangeListener* listener);
  void RemoveListener(PowerSettingChangeListener* listener);

 private:
  friend struct base::DefaultSingletonTraits<PowerSettingChangeObserver>;

  PowerSettingChangeObserver();
  virtual ~PowerSettingChangeObserver();

  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  void OnDisplayStateChanged(bool display_on);
  void OnResume();
  void OnSuspend();

  static HPOWERNOTIFY RegisterNotification(LPCGUID power_setting);
  static BOOL UnregisterNotification(HPOWERNOTIFY handle);

  base::ObserverList<PowerSettingChangeListener>::Unchecked listeners_;
  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;
  HPOWERNOTIFY power_display_state_;
};

// static
PowerSettingChangeObserver* PowerSettingChangeObserver::GetInstance() {
  return base::Singleton<PowerSettingChangeObserver>::get();
}

PowerSettingChangeObserver::PowerSettingChangeObserver()
    : singleton_hwnd_observer_(new gfx::SingletonHwndObserver(
          base::BindRepeating(&PowerSettingChangeObserver::OnWndProc,
                              base::Unretained(this)))),
      power_display_state_(RegisterNotification(&GUID_SESSION_DISPLAY_STATUS)) {
}

PowerSettingChangeObserver::~PowerSettingChangeObserver() {
  UnregisterNotification(power_display_state_);
}

void PowerSettingChangeObserver::AddListener(
    PowerSettingChangeListener* listener) {
  listeners_.AddObserver(listener);
}

void PowerSettingChangeObserver::RemoveListener(
    PowerSettingChangeListener* listener) {
  listeners_.RemoveObserver(listener);
}

void PowerSettingChangeObserver::OnWndProc(HWND hwnd,
                                           UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam) {
  if (message == WM_POWERBROADCAST) {
    switch (wparam) {
      case PBT_POWERSETTINGCHANGE: {
        POWERBROADCAST_SETTING* setting = (POWERBROADCAST_SETTING*)lparam;
        if (setting &&
            IsEqualGUID(setting->PowerSetting, GUID_SESSION_DISPLAY_STATUS) &&
            setting->DataLength == sizeof(DWORD)) {
          OnDisplayStateChanged(
              PowerMonitorOff !=
              static_cast<MONITOR_DISPLAY_STATE>(setting->Data[0]));
        }
      } break;
      case PBT_APMRESUMEAUTOMATIC:
        OnResume();
        break;
      case PBT_APMSUSPEND:
        OnSuspend();
        break;
      default:
        return;
    }
  }
}

void PowerSettingChangeObserver::OnResume() {
  listeners_.Notify(&PowerSettingChangeListener::OnResume);
}

void PowerSettingChangeObserver::OnSuspend() {
  listeners_.Notify(&PowerSettingChangeListener::OnSuspend);
}

void PowerSettingChangeObserver::OnDisplayStateChanged(bool display_on) {
  listeners_.Notify(&PowerSettingChangeListener::OnDisplayStateChanged,
                    display_on);
}

HPOWERNOTIFY PowerSettingChangeObserver::RegisterNotification(
    LPCGUID power_setting) {
  return RegisterPowerSettingNotification(
      gfx::SingletonHwnd::GetInstance()->hwnd(), power_setting,
      DEVICE_NOTIFY_WINDOW_HANDLE);
}

BOOL PowerSettingChangeObserver::UnregisterNotification(HPOWERNOTIFY handle) {
  return UnregisterPowerSettingNotification(handle);
}

ScopedPowerSettingChangeListener::ScopedPowerSettingChangeListener(
    PowerSettingChangeListener* listener)
    : listener_(listener) {
  PowerSettingChangeObserver::GetInstance()->AddListener(listener_);
}

ScopedPowerSettingChangeListener::~ScopedPowerSettingChangeListener() {
  PowerSettingChangeObserver::GetInstance()->RemoveListener(listener_);
}

}  // namespace ui
