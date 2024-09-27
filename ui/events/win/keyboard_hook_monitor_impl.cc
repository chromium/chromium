// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/win/keyboard_hook_monitor_impl.h"

#include "base/no_destructor.h"

namespace ui {

KeyboardHookMonitorImpl::KeyboardHookMonitorImpl() = default;

KeyboardHookMonitorImpl::~KeyboardHookMonitorImpl() = default;

void KeyboardHookMonitorImpl::AddObserver(KeyboardHookObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.AddObserver(observer);
}

void KeyboardHookMonitorImpl::RemoveObserver(KeyboardHookObserver* observer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  observers_.RemoveObserver(observer);
}

bool KeyboardHookMonitorImpl::IsActive() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return is_hook_active_;
}

void KeyboardHookMonitorImpl::NotifyHookRegistered() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!is_hook_active_);

  is_hook_active_ = true;
  observers_.Notify(&KeyboardHookObserver::OnHookRegistered);
}

void KeyboardHookMonitorImpl::NotifyHookUnregistered() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_hook_active_);

  is_hook_active_ = false;
  observers_.Notify(&KeyboardHookObserver::OnHookUnregistered);
}

// static
KeyboardHookMonitor* KeyboardHookMonitor::GetInstance() {
  return reinterpret_cast<KeyboardHookMonitor*>(
      KeyboardHookMonitorImpl::GetInstance());
}

// static
KeyboardHookMonitorImpl* KeyboardHookMonitorImpl::GetInstance() {
  static base::NoDestructor<KeyboardHookMonitorImpl> instance;
  return instance.get();
}

}  // namespace ui
