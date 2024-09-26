// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/sys_color_change_listener.h"

#include <windows.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace gfx {

class SysColorChangeObserver {
 public:
  static SysColorChangeObserver* GetInstance();

  void AddListener(SysColorChangeListener* listener);
  void RemoveListener(SysColorChangeListener* listener);

 private:
  friend struct base::DefaultSingletonTraits<SysColorChangeObserver>;

  SysColorChangeObserver();
  virtual ~SysColorChangeObserver();

  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  base::ObserverList<SysColorChangeListener>::Unchecked listeners_;
  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;
};

// static
SysColorChangeObserver* SysColorChangeObserver::GetInstance() {
  return base::Singleton<SysColorChangeObserver>::get();
}

SysColorChangeObserver::SysColorChangeObserver()
    : singleton_hwnd_observer_(new SingletonHwndObserver(
          base::BindRepeating(&SysColorChangeObserver::OnWndProc,
                              base::Unretained(this)))) {}

SysColorChangeObserver::~SysColorChangeObserver() {}

void SysColorChangeObserver::AddListener(SysColorChangeListener* listener) {
  listeners_.AddObserver(listener);
}

void SysColorChangeObserver::RemoveListener(SysColorChangeListener* listener) {
  listeners_.RemoveObserver(listener);
}

void SysColorChangeObserver::OnWndProc(HWND hwnd,
                                       UINT message,
                                       WPARAM wparam,
                                       LPARAM lparam) {
  if (message == WM_SYSCOLORCHANGE ||
      (message == WM_SETTINGCHANGE && wparam == SPI_SETHIGHCONTRAST)) {
    listeners_.Notify(&SysColorChangeListener::OnSysColorChange);
  }
}

ScopedSysColorChangeListener::ScopedSysColorChangeListener(
    SysColorChangeListener* listener)
    : listener_(listener) {
  SysColorChangeObserver::GetInstance()->AddListener(listener_);
}

ScopedSysColorChangeListener::~ScopedSysColorChangeListener() {
  SysColorChangeObserver::GetInstance()->RemoveListener(listener_);
}

}  // namespace gfx
