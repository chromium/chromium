// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/native_view_focus_manager.h"

#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace views {

// NativeViewFocusManager ----------------------------------------------------------

// static
NativeViewFocusManager* NativeViewFocusManager::GetInstance() {
  static base::NoDestructor<NativeViewFocusManager> instance;
  return instance.get();
}

NativeViewFocusManager::~NativeViewFocusManager() = default;

void NativeViewFocusManager::AddFocusChangeListener(
    NativeViewFocusChangeListener* listener) {
  focus_change_listeners_.AddObserver(listener);
}

void NativeViewFocusManager::RemoveFocusChangeListener(
    NativeViewFocusChangeListener* listener) {
  focus_change_listeners_.RemoveObserver(listener);
}

void NativeViewFocusManager::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (enabled_) {
    focus_change_listeners_.Notify(
        &NativeViewFocusChangeListener::OnNativeFocusChanged, focused_now);
  }
}

NativeViewFocusManager::NativeViewFocusManager() = default;

// AutoNativeNotificationDisabler ----------------------------------------------

AutoNativeNotificationDisabler::AutoNativeNotificationDisabler() {
  NativeViewFocusManager::GetInstance()->DisableNotifications();
}

AutoNativeNotificationDisabler::~AutoNativeNotificationDisabler() {
  NativeViewFocusManager::GetInstance()->EnableNotifications();
}

}  // namespace views
