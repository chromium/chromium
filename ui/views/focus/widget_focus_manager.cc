// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/widget_focus_manager.h"

namespace views {

// WidgetFocusManager ----------------------------------------------------------

// static
WidgetFocusManager* WidgetFocusManager::GetInstance() {
  static base::NoDestructor<WidgetFocusManager> instance;
  return instance.get();
}

WidgetFocusManager::~WidgetFocusManager() = default;

void WidgetFocusManager::AddFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  focus_change_listeners_.AddObserver(listener);
}

void WidgetFocusManager::RemoveFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  focus_change_listeners_.RemoveObserver(listener);
}

void WidgetFocusManager::OnNativeFocusChanged(gfx::NativeView focused_now) {
  if (enabled_) {
    for (WidgetFocusChangeListener& observer : focus_change_listeners_)
      observer.OnNativeFocusChanged(focused_now);
  }
}

WidgetFocusManager::WidgetFocusManager() = default;

// AutoNativeNotificationDisabler ----------------------------------------------

AutoNativeNotificationDisabler::AutoNativeNotificationDisabler() {
  WidgetFocusManager::GetInstance()->DisableNotifications();
}

AutoNativeNotificationDisabler::~AutoNativeNotificationDisabler() {
  WidgetFocusManager::GetInstance()->EnableNotifications();
}

}  // namespace views
